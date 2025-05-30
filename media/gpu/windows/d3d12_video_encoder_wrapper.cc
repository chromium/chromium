// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d12_video_encoder_wrapper.h"

#include <algorithm>

#include "media/base/win/mf_helpers.h"
#include "media/gpu/windows/d3d12_helpers.h"
#include "media/gpu/windows/d3d12_video_helpers.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_barriers.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3dx12_core.h"

namespace media {

namespace {

uint64_t GetMaxResolvedMetadataBufferSize(D3D12_VIDEO_ENCODER_CODEC codec,
                                          uint32_t max_subregions_number) {
  // https://microsoft.github.io/DirectX-Specs/d3d/D3D12_Video_Encoding_AV1.html#resolved-buffer-layouts-for-resolveencoderoutputmetadata
  switch (codec) {
    case D3D12_VIDEO_ENCODER_CODEC_H264:
    case D3D12_VIDEO_ENCODER_CODEC_HEVC:
      return sizeof(D3D12_VIDEO_ENCODER_OUTPUT_METADATA) +
             (max_subregions_number *
              sizeof(D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA));
    case D3D12_VIDEO_ENCODER_CODEC_AV1:
      return sizeof(D3D12_VIDEO_ENCODER_OUTPUT_METADATA) +
             (max_subregions_number *
              sizeof(D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA)) +
             sizeof(
                 D3D12_VIDEO_ENCODER_AV1_PICTURE_CONTROL_SUBREGIONS_LAYOUT_DATA_TILES) +
             sizeof(D3D12_VIDEO_ENCODER_AV1_POST_ENCODE_VALUES);
    default:
      NOTREACHED();
  }
}

}  // namespace

D3D12VideoEncoderWrapper::D3D12VideoEncoderWrapper(
    Microsoft::WRL::ComPtr<ID3D12VideoEncoder> video_encoder,
    Microsoft::WRL::ComPtr<ID3D12VideoEncoderHeap> video_encoder_heap)
    : video_encoder_(std::move(video_encoder)),
      video_encoder_heap_(std::move(video_encoder_heap)) {}

D3D12VideoEncoderWrapper::~D3D12VideoEncoderWrapper() = default;

bool D3D12VideoEncoderWrapper::Initialize(uint32_t max_subregions_number) {
  CHECK(video_encoder_);
  CHECK(video_encoder_heap_);
  Microsoft::WRL::ComPtr<ID3D12Device> device;
  CHECK_EQ(video_encoder_->GetDevice(IID_PPV_ARGS(&device)), S_OK);
  Microsoft::WRL::ComPtr<ID3D12VideoDevice3> video_device3;
  CHECK_EQ(device.As(&video_device3), S_OK);

  // Get the profile, resolution and MaxEncoderOutputMetadataBufferSize
  D3D12_VIDEO_ENCODER_CODEC codec = video_encoder_->GetCodec();
  D3D12_VIDEO_ENCODER_PROFILE_DESC profile_desc{};
  switch (codec) {
    case D3D12_VIDEO_ENCODER_CODEC_H264:
      profile_desc = {
          .DataSize = sizeof(profile_data_.h264_profile_),
          .pH264Profile = &profile_data_.h264_profile_,
      };
      break;
    case D3D12_VIDEO_ENCODER_CODEC_HEVC:
      profile_desc = {
          .DataSize = sizeof(profile_data_.hevc_profile_),
          .pHEVCProfile = &profile_data_.hevc_profile_,
      };
      break;
    case D3D12_VIDEO_ENCODER_CODEC_AV1:
      profile_desc = {
          .DataSize = sizeof(profile_data_.av1_profile_),
          .pAV1Profile = &profile_data_.av1_profile_,
      };
      break;
    default:
      NOTREACHED();
  }
  HRESULT hr = video_encoder_->GetCodecProfile(profile_desc);
  RETURN_ON_HR_FAILURE(hr, "GetCodecProfile failed", false);
  CHECK_EQ(video_encoder_heap_->GetResolutionListCount(), 1u);
  D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC resolution_desc;
  hr = video_encoder_heap_->GetResolutionList(1, &resolution_desc);
  RETURN_ON_HR_FAILURE(hr, "GetResolutionList failed", false);
  D3D12_FEATURE_DATA_VIDEO_ENCODER_RESOURCE_REQUIREMENTS
  resource_requirements = {
      .Codec = codec,
      .Profile = profile_desc,
      .InputFormat = video_encoder_->GetInputFormat(),
      .PictureTargetResolution = resolution_desc,
  };
  EncoderStatus result = CheckD3D12VideoEncoderResourceRequirements(
      video_device3.Get(), &resource_requirements);
  if (!result.is_ok()) {
    // We should have checked it before creating the encoder.
    CHECK_NE(result.code(), EncoderStatus::Codes::kEncoderUnsupportedConfig);
    return false;
  }

  D3D12_COMMAND_QUEUE_DESC command_queue_desc{
      D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE};
  // TODO(crbug.com/40275246): Share command queues across encoders
  hr = device->CreateCommandQueue(&command_queue_desc,
                                  IID_PPV_ARGS(&command_queue_));
  RETURN_ON_HR_FAILURE(hr, "CreateCommandQueue failed", false);

  hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE,
                                      IID_PPV_ARGS(&command_allocator_));
  RETURN_ON_HR_FAILURE(hr, "CreateCommandAllocator failed", false);

  hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE,
                                 command_allocator_.Get(), nullptr,
                                 IID_PPV_ARGS(&command_list_));
  RETURN_ON_HR_FAILURE(hr, "CreateCommandList failed", false);
  hr = command_list_->Close();
  RETURN_ON_HR_FAILURE(hr, "Close command list failed", false);

  fence_ = D3D12Fence::Create(device.Get());
  if (!fence_) {
    return false;
  }

  // A NV12 format frame consists of a Y-plane which occupies the same size as
  // the frame itself, and an UV-plane which is half the size of the frame.
  // A buffer of 1 + 1/2 = 3/2 times the size of the frame bytes should be
  // enough for a compressed bitstream.
  CD3DX12_RESOURCE_DESC bitstream_desc = CD3DX12_RESOURCE_DESC::Buffer(
      resolution_desc.Width * resolution_desc.Height * 3 / 2);
  hr = device->CreateCommittedResource(&D3D12HeapProperties::kReadback,
                                       D3D12_HEAP_FLAG_NONE, &bitstream_desc,
                                       D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                       IID_PPV_ARGS(&bitstream_buffer_));
  RETURN_ON_HR_FAILURE(
      hr, "CreateCommittedResource for bitstream buffer failed", false);

  CD3DX12_RESOURCE_DESC opaque_metadata_desc = CD3DX12_RESOURCE_DESC::Buffer(
      resource_requirements.MaxEncoderOutputMetadataBufferSize);
  hr = device->CreateCommittedResource(
      &D3D12HeapProperties::kDefault, D3D12_HEAP_FLAG_NONE,
      &opaque_metadata_desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS(&opaque_metadata_buffer_));
  RETURN_ON_HR_FAILURE(
      hr, "CreateCommittedResource for opaque metadata buffer failed", false);

  CD3DX12_RESOURCE_DESC metadata_desc = CD3DX12_RESOURCE_DESC::Buffer(
      GetMaxResolvedMetadataBufferSize(codec, max_subregions_number));
  hr = device->CreateCommittedResource(
      &D3D12HeapProperties::kReadback, D3D12_HEAP_FLAG_NONE, &metadata_desc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&metadata_buffer_));
  RETURN_ON_HR_FAILURE(hr, "CreateCommittedResource for metadata buffer failed",
                       false);

  output_arguments_ = {
      .Bitstream = {bitstream_buffer_.Get()},
      .EncoderOutputMetadata = {opaque_metadata_buffer_.Get()},
  };
  resolve_metadata_input_arguments_ = {
      .EncoderCodec = video_encoder_->GetCodec(),
      .EncoderProfile = profile_desc,
      .EncoderInputFormat = video_encoder_->GetInputFormat(),
      .EncodedPictureEffectiveResolution = resolution_desc,
      .HWLayoutMetadata = {.pBuffer = opaque_metadata_buffer_.Get()},
  };
  resolve_metadata_output_arguments_ = {
      .ResolvedLayoutMetadata = {.pBuffer = metadata_buffer_.Get()},
  };

  return true;
}

EncoderStatus D3D12VideoEncoderWrapper::Encode(
    const D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS& input_arguments,
    const D3D12_VIDEO_ENCODER_RECONSTRUCTED_PICTURE& reconstructed_picture) {
  HRESULT hr = command_allocator_->Reset();
  RETURN_ON_HR_FAILURE(hr, "Failed to Reset video_encode_command_allocator_",
                       EncoderStatus::Codes::kSystemAPICallError);
  hr = command_list_->Reset(command_allocator_.Get());
  RETURN_ON_HR_FAILURE(hr, "Failed to Reset video_encode_command_list_",
                       EncoderStatus::Codes::kSystemAPICallError);

  // Encode frame

  std::vector<D3D12_RESOURCE_BARRIER> encode_frame_barriers{
      CD3DX12_RESOURCE_BARRIER::Transition(
          opaque_metadata_buffer_.Get(), D3D12_RESOURCE_STATE_COMMON,
          D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE)};
  auto input_frame_barriers = CreateD3D12TransitionBarriersForAllPlanes(
      input_arguments.pInputFrame, input_arguments.InputFrameSubresource,
      D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ);
  encode_frame_barriers.insert(encode_frame_barriers.end(),
                               input_frame_barriers.begin(),
                               input_frame_barriers.end());
  if (reconstructed_picture.pReconstructedPicture) {
    auto reconstructed_picture_barriers =
        CreateD3D12TransitionBarriersForAllPlanes(
            reconstructed_picture.pReconstructedPicture,
            reconstructed_picture.ReconstructedPictureSubresource,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE);
    encode_frame_barriers.insert(encode_frame_barriers.end(),
                                 reconstructed_picture_barriers.begin(),
                                 reconstructed_picture_barriers.end());
  }
  const D3D12_VIDEO_ENCODE_REFERENCE_FRAMES& reference_frames =
      input_arguments.PictureControlDesc.ReferenceFrames;
  for (size_t i = 0; i < reference_frames.NumTexture2Ds; i++) {
    auto reference_frame_barriers = CreateD3D12TransitionBarriersForAllPlanes(
        // SAFETY: D3D12ReferenceFrameList guarantees that |.ppTexture2Ds| and
        // |.pSubresources| have at least |.NumTexture2Ds| elements.
        UNSAFE_BUFFERS(reference_frames.ppTexture2Ds[i]),
        UNSAFE_BUFFERS(reference_frames.pSubresources[i]),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ);
    encode_frame_barriers.insert(encode_frame_barriers.end(),
                                 reference_frame_barriers.begin(),
                                 reference_frame_barriers.end());
  }
  command_list_->ResourceBarrier(encode_frame_barriers.size(),
                                 encode_frame_barriers.data());

  output_arguments_.ReconstructedPicture = reconstructed_picture;
  command_list_->EncodeFrame(video_encoder_.Get(), video_encoder_heap_.Get(),
                             &input_arguments, &output_arguments_);

  for (D3D12_RESOURCE_BARRIER& barrier : encode_frame_barriers) {
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
  }
  // Transit the opaque_metadata_buffer_'s state directly to video-encode-read
  // for the following call.
  encode_frame_barriers[0].Transition.StateAfter =
      D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ;
  command_list_->ResourceBarrier(encode_frame_barriers.size(),
                                 encode_frame_barriers.data());

  // ResolveEncoderOutputMetadata

  command_list_->ResolveEncoderOutputMetadata(
      &resolve_metadata_input_arguments_, &resolve_metadata_output_arguments_);

  D3D12_RESOURCE_BARRIER resolve_encoder_output_metadata_barrier =
      CD3DX12_RESOURCE_BARRIER::Transition(
          opaque_metadata_buffer_.Get(), D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ,
          D3D12_RESOURCE_STATE_COMMON);
  command_list_->ResourceBarrier(1, &resolve_encoder_output_metadata_barrier);

  hr = command_list_->Close();
  RETURN_ON_HR_FAILURE(hr, "Failed to Close video_encode_command_list_",
                       EncoderStatus::Codes::kSystemAPICallError);

  // Execution

  ID3D12CommandList* command_lists[] = {command_list_.Get()};
  command_queue_->ExecuteCommandLists(std::size(command_lists), command_lists);
  return fence_->SignalAndWait(*command_queue_.Get()) == D3D11StatusCode::kOk
             ? EncoderStatus::Codes::kOk
             : EncoderStatus::Codes::kSystemAPICallError;
}

EncoderStatus::Or<ScopedD3D12ResourceMap>
D3D12VideoEncoderWrapper::GetEncoderOutputMetadata() const {
  ScopedD3D12ResourceMap mapped_metadata;
  if (!mapped_metadata.Map(metadata_buffer_.Get(), 0, nullptr)) {
    return EncoderStatus::Codes::kSystemAPICallError;
  }
  if (mapped_metadata.data().size() <
      sizeof(D3D12_VIDEO_ENCODER_OUTPUT_METADATA)) {
    return {EncoderStatus::Codes::kSystemAPICallError, "Bad mapped metadata"};
  }
  D3D12_VIDEO_ENCODER_OUTPUT_METADATA* metadata =
      reinterpret_cast<D3D12_VIDEO_ENCODER_OUTPUT_METADATA*>(
          mapped_metadata.data().data());
  if (metadata->EncodeErrorFlags) {
    LOG(ERROR) << "D3D12VideoEncoder got non-zero EncodeErrorFlags";
    if (metadata->EncodeErrorFlags &
        D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_CODEC_PICTURE_CONTROL_NOT_SUPPORTED) {
      LOG(ERROR) << "D3D12VideoEncoder does not support codec picture control";
    }
    if (metadata->EncodeErrorFlags &
        D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_SUBREGION_LAYOUT_CONFIGURATION_NOT_SUPPORTED) {
      LOG(ERROR) << "D3D12VideoEncoder does not support subregion layout "
                    "configuration";
    }
    if (metadata->EncodeErrorFlags &
        D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_INVALID_REFERENCE_PICTURES) {
      LOG(ERROR) << "D3D12VideoEncoder received invalid reference pictures";
    }
    if (metadata->EncodeErrorFlags &
        D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_RECONFIGURATION_REQUEST_NOT_SUPPORTED) {
      LOG(ERROR)
          << "D3D12VideoEncoder does not support reconfiguration request";
    }
    if (metadata->EncodeErrorFlags &
        D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_INVALID_METADATA_BUFFER_SOURCE) {
      LOG(ERROR) << "D3D12VideoEncoder received invalid metadata buffer source";
    }
    return EncoderStatus::Codes::kEncoderUnsupportedConfig;
  }
  return mapped_metadata;
}

EncoderStatus D3D12VideoEncoderWrapper::ReadbackBitstream(
    base::span<uint8_t> data) const {
  D3D12_RANGE bitstream_read_range{0, data.size()};
  ScopedD3D12ResourceMap mapped_bitstream;
  if (!mapped_bitstream.Map(bitstream_buffer_.Get(), 0,
                            &bitstream_read_range)) {
    return EncoderStatus::Codes::kSystemAPICallError;
  }
  data.copy_from(mapped_bitstream.data().first(data.size()));
  D3D12_RANGE bitstream_written_range{0, 0};
  mapped_bitstream.Commit(&bitstream_written_range);
  return EncoderStatus::Codes::kOk;
}

}  // namespace media
