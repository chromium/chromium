// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_helpers.h"

#include "base/check_is_test.h"
#include "base/logging.h"
#include "media/base/video_codecs.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/format_utils.h"
#include "media/gpu/windows/supported_profile_helpers.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_core.h"

namespace media {

D3D12PictureBuffer::D3D12PictureBuffer(
    const Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
    UINT subresource,
    const D3D12FenceAndValue& fence_and_value)
    : resource(resource),
      subresource(subresource),
      fence_and_value(fence_and_value) {}

D3D12PictureBuffer::~D3D12PictureBuffer() = default;

D3D12PictureBuffer::D3D12PictureBuffer(const D3D12PictureBuffer& other) =
    default;
D3D12PictureBuffer::D3D12PictureBuffer(D3D12PictureBuffer&& other) noexcept =
    default;
D3D12PictureBuffer& D3D12PictureBuffer::operator=(
    const D3D12PictureBuffer& other) = default;
D3D12PictureBuffer& D3D12PictureBuffer::operator=(
    D3D12PictureBuffer&& other) noexcept = default;

D3D12ReferenceFrameList::D3D12ReferenceFrameList(ComD3D12VideoDecoderHeap heap)
    : heap_(std::move(heap)) {
  std::fill(heaps_.begin(), heaps_.end(), heap_.Get());
}

D3D12ReferenceFrameList::~D3D12ReferenceFrameList() = default;

D3D12ReferenceFrameList::D3D12ReferenceFrameList(
    const D3D12ReferenceFrameList& other) = default;

void D3D12ReferenceFrameList::SetPictureBuffers(
    base::span<scoped_refptr<D3D11PictureBuffer>> picture_buffers) {
  for (size_t i = 0; i < picture_buffers.size(); i++) {
    picture_buffers_[i] = picture_buffers[i].get();
  }
}

void D3D12ReferenceFrameList::WriteTo(
    D3D12_VIDEO_DECODE_REFERENCE_FRAMES* dest) {
  dest->NumTexture2Ds = static_cast<UINT>(size_);
  dest->ppTexture2Ds = resources_.data();
  dest->pSubresources = subresources_.data();
  dest->ppHeaps = heaps_.data();
}

void D3D12ReferenceFrameList::emplace(size_t index,
                                      ID3D12Resource* resource,
                                      UINT subresource) {
  if (index >= size_) {
    size_ = index + 1;
    CHECK_LE(size_, kMaxSize);
  }
  resources_[index] = resource;
  subresources_[index] = subresource;
}

std::vector<D3D12_RESOURCE_BARRIER>
D3D12ReferenceFrameList::GetTransitionsToDecodeState(
    ID3D12Resource* current_output_resource,
    UINT current_output_subresource) {
  std::vector<D3D12_RESOURCE_BARRIER> barriers;
  for (size_t i = 0; i < size_; i++) {
    if (resources_[i] == current_output_resource &&
        subresources_[i] == current_output_subresource) {
      auto transitions = CreateD3D12TransitionBarriersForAllPlanes(
          resources_[i], subresources_[i], D3D12_RESOURCE_STATE_COMMON,
          D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE);
      barriers.insert(barriers.end(), transitions.begin(), transitions.end());
    } else if (picture_buffers_[i]->in_picture_use()) {
      auto transitions = CreateD3D12TransitionBarriersForAllPlanes(
          resources_[i], subresources_[i], D3D12_RESOURCE_STATE_COMMON,
          D3D12_RESOURCE_STATE_VIDEO_DECODE_READ);
      barriers.insert(barriers.end(), transitions.begin(), transitions.end());
    }
  }
  return barriers;
}

ScopedD3D12ResourceMap::ScopedD3D12ResourceMap() = default;

ScopedD3D12ResourceMap::~ScopedD3D12ResourceMap() {
  Commit();
}

ScopedD3D12ResourceMap::ScopedD3D12ResourceMap(
    ScopedD3D12ResourceMap&& other) noexcept = default;

ScopedD3D12ResourceMap& ScopedD3D12ResourceMap::operator=(
    ScopedD3D12ResourceMap&& other) noexcept = default;

bool ScopedD3D12ResourceMap::Map(ID3D12Resource* resource,
                                 UINT subresource,
                                 const D3D12_RANGE* read_range) {
  CHECK(data_.empty());
  CHECK(resource);
  CHECK_EQ(resource->GetDesc().Dimension, D3D12_RESOURCE_DIMENSION_BUFFER);
  void* data;
  HRESULT hr = resource->Map(subresource, read_range, &data);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to Map D3D12Resource: "
               << logging::SystemErrorCodeToString(hr);
    return false;
  }
  resource_ = resource;
  subresource_ = subresource;
  // SAFETY: A successful |ID3D12Resource::Map()| sets |data| with valid
  // address, and for D3D12_RESOURCE_DIMENSION_BUFFER resource, the length is
  // its |Width|. We will also reset the |data_| before we |Unmap()|.
  data_ = UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(data),
                 static_cast<size_t>(resource_->GetDesc().Width)));
  return true;
}

void ScopedD3D12ResourceMap::Commit(const D3D12_RANGE* written_range) {
  if (resource_) {
    data_ = {};
    resource_->Unmap(subresource_, written_range);
    resource_ = nullptr;
  }
}

ComD3D12Device CreateD3D12Device(IDXGIAdapter* adapter) {
  if (!adapter) {
    // We've had at least a couple of scenarios where two calls to EnumAdapters
    // return different default adapters on multi-adapter systems due to race
    // conditions. These result in hard to repro bugs for end users.
    // Allowing using a default adapter only in tests.
    CHECK_IS_TEST();
  }

  ComD3D12Device device;
  HRESULT hr =
      D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
  if (FAILED(hr)) {
    LOG(ERROR) << "D3D12CreateDevice failed: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return device;
}

absl::InlinedVector<D3D12_RESOURCE_BARRIER, 2>
CreateD3D12TransitionBarriersForAllPlanes(ID3D12Resource* resource,
                                          UINT subresource,
                                          D3D12_RESOURCE_STATES state_before,
                                          D3D12_RESOURCE_STATES state_after) {
  CHECK(resource);
  D3D12_RESOURCE_DESC desc = resource->GetDesc();
  absl::InlinedVector<D3D12_RESOURCE_BARRIER, 2> barriers;
  for (size_t i = 0; i < GetFormatPlaneCount(desc.Format); i++) {
    barriers.push_back({.Transition = {.pResource = resource,
                                       .Subresource = D3D12CalcSubresource(
                                           subresource, 0, i, desc.MipLevels,
                                           desc.DepthOrArraySize),
                                       .StateBefore = state_before,
                                       .StateAfter = state_after}});
  }
  return barriers;
}

GUID GetD3D12VideoDecodeGUID(VideoCodecProfile profile,
                             uint8_t bitdepth,
                             VideoChromaSampling chroma_sampling) {
  switch (profile) {
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_EXTENDED:
    case H264PROFILE_HIGH:
    case H264PROFILE_HIGH10PROFILE:
    case H264PROFILE_HIGH422PROFILE:
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
    case H264PROFILE_SCALABLEBASELINE:
    case H264PROFILE_SCALABLEHIGH:
    case H264PROFILE_STEREOHIGH:
    case H264PROFILE_MULTIVIEWHIGH:
      return D3D12_VIDEO_DECODE_PROFILE_H264;
    case VP9PROFILE_PROFILE0:
      return D3D12_VIDEO_DECODE_PROFILE_VP9;
    case VP9PROFILE_PROFILE2:
      return D3D12_VIDEO_DECODE_PROFILE_VP9_10BIT_PROFILE2;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    // Per DirectX Video Acceleration Specification for High Efficiency Video
    // Coding - 7.4, DXVA_ModeHEVC_VLD_Main GUID can be used for both main and
    // main still picture profile.
    case HEVCPROFILE_MAIN:
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN;
    case HEVCPROFILE_MAIN10:
      return D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN10;
    case HEVCPROFILE_REXT:
      return GetHEVCRangeExtensionGUID(bitdepth, chroma_sampling,
                                       /*use_dxva_device_for_hevc_rext=*/true);
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case AV1PROFILE_PROFILE_MAIN:
      return D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE0;
    case AV1PROFILE_PROFILE_HIGH:
      return D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE1;
    case AV1PROFILE_PROFILE_PRO:
      return D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE2;
    default:
      return {};
  }
}

D3D11To12Fence::D3D11To12Fence(Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence,
                               Microsoft::WRL::ComPtr<ID3D12Fence> d3d12_fence)
    : d3d11_fence_(std::move(d3d11_fence)),
      d3d12_fence_(std::move(d3d12_fence)),
      fence_value_(0) {}

D3D11To12Fence::~D3D11To12Fence() = default;

}  // namespace media
