// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_HELPERS_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_HELPERS_H_

#include "third_party/microsoft_dxheaders/src/include/directx/d3d12video.h"
// Windows SDK headers should be included after DirectX headers.

#include "media/base/encoder_status.h"
#include "media/gpu/windows/d3d12_video_encoder_wrapper.h"

namespace media {

EncoderStatus CheckD3D12VideoEncoderCodec(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC* codec);

// Get the supported configurations for the codec and profile. This method
// handles the special variation of the usages for HEVC documented in
// https://learn.microsoft.com/en-us/windows/win32/api/d3d12video/ns-d3d12video-d3d12_feature_data_video_encoder_codec_configuration_support#members
// and always returns a supported configuration if there is any.
EncoderStatus CheckD3D12VideoEncoderCodecConfigurationSupport(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC_CONFIGURATION_SUPPORT* support);

EncoderStatus CheckD3D12VideoEncoderInputFormat(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_INPUT_FORMAT* input_format);

EncoderStatus CheckD3D12VideoEncoderProfileLevel(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_PROFILE_LEVEL* profile_level);

EncoderStatus CheckD3D12VideoEncoderResourceRequirements(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_RESOURCE_REQUIREMENTS*
        resource_requirements);

EncoderStatus CheckD3D12VideoEncoderSupport(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT* support);

EncoderStatus CheckD3D12VideoEncoderSupport1(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_SUPPORT1* support);

std::unique_ptr<D3D12VideoEncoderWrapper> CreateD3D12VideoEncoderWrapper(
    ID3D12VideoDevice* video_device,
    D3D12_VIDEO_ENCODER_CODEC codec,
    const D3D12_VIDEO_ENCODER_PROFILE_DESC& profile,
    const D3D12_VIDEO_ENCODER_LEVEL_SETTING& level,
    DXGI_FORMAT input_format,
    const D3D12_VIDEO_ENCODER_CODEC_CONFIGURATION& codec_config,
    const D3D12_VIDEO_ENCODER_PICTURE_RESOLUTION_DESC& resolution);

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_HELPERS_H_
