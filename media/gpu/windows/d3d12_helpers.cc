// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_helpers.h"

#include "base/check_is_test.h"
#include "base/logging.h"
#include "media/base/video_codecs.h"
#include "media/gpu/windows/format_utils.h"
#include "media/gpu/windows/supported_profile_helpers.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_core.h"

namespace media {

D3D12ReferenceFrameList::D3D12ReferenceFrameList(ComD3D12VideoDecoderHeap heap)
    : heap_(std::move(heap)) {
  std::fill(heaps_.begin(), heaps_.end(), heap_.Get());
}

D3D12ReferenceFrameList::~D3D12ReferenceFrameList() = default;

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
      return GetHEVCRangeExtensionPrivateGUID(bitdepth, chroma_sampling);
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

}  // namespace media
