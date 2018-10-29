// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_video_decoder_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "content/renderer/media/webrtc/rtc_video_decoder.h"
#include "content/renderer/media/webrtc/rtc_video_decoder_adapter.h"
#include "media/base/media_switches.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace content {

RTCVideoDecoderFactory::RTCVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories) {
  DVLOG(2) << __func__;
}

RTCVideoDecoderFactory::~RTCVideoDecoderFactory() {
  DVLOG(2) << __func__;
}

webrtc::VideoDecoder* RTCVideoDecoderFactory::CreateVideoDecoder(
    webrtc::VideoCodecType type) {
  DVLOG(2) << __func__;

  if (base::FeatureList::IsEnabled(media::kRTCVideoDecoderAdapter)) {
    return RTCVideoDecoderAdapter::Create(gpu_factories_, type).release();
  } else {
    return RTCVideoDecoder::Create(type, gpu_factories_).release();
  }
}

void RTCVideoDecoderFactory::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
  DVLOG(2) << __func__;
  gpu_factories_->GetTaskRunner()->DeleteSoon(FROM_HERE, decoder);
}

}  // namespace content
