// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_factory.h"

#include "base/single_thread_task_runner.h"

namespace media {

DecoderFactory::DecoderFactory() = default;

DecoderFactory::~DecoderFactory() = default;

void DecoderFactory::CreateAudioDecoders(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) {}

SupportedVideoDecoderConfigs
DecoderFactory::GetSupportedVideoDecoderConfigsForWebRTC() {
  return {};
}

void DecoderFactory::CreateVideoDecoders(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) {}

}  // namespace media
