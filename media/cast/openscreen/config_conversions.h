// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_OPENSCREEN_CONFIG_CONVERSIONS_H_
#define MEDIA_CAST_OPENSCREEN_CONFIG_CONVERSIONS_H_

#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "third_party/openscreen/src/cast/streaming/capture_configs.h"

namespace media::cast {

// Utility functions to convert between media and Open Screen types.

openscreen::cast::AudioCaptureConfig ToAudioCaptureConfig(
    const media::AudioDecoderConfig& audio_config);

openscreen::cast::VideoCaptureConfig ToVideoCaptureConfig(
    const media::VideoDecoderConfig& video_config);

media::AudioDecoderConfig ToAudioDecoderConfig(
    const openscreen::cast::AudioCaptureConfig& audio_capture_config);

media::VideoDecoderConfig ToVideoDecoderConfig(
    const openscreen::cast::VideoCaptureConfig& video_capture_config);

openscreen::cast::AudioCodec ToAudioCaptureConfigCodec(media::AudioCodec codec);

openscreen::cast::VideoCodec ToVideoCaptureConfigCodec(media::VideoCodec codec);

}  // namespace media::cast

#endif  // MEDIA_CAST_OPENSCREEN_CONFIG_CONVERSIONS_H_
