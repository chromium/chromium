// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D12_VIDEO_HELPERS_H_
#define MEDIA_GPU_WINDOWS_D3D12_VIDEO_HELPERS_H_

#include <d3d12video.h>

#include "media/base/encoder_status.h"

namespace media {

EncoderStatus CheckD3D12VideoEncoderCodec(
    ID3D12VideoDevice* video_device,
    D3D12_FEATURE_DATA_VIDEO_ENCODER_CODEC* codec);

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

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D12_VIDEO_HELPERS_H_
