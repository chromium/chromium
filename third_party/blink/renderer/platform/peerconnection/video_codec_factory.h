// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_VIDEO_CODEC_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_VIDEO_CODEC_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/peerconnection/stats_collector.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"

namespace media {
class GpuVideoAcceleratorFactories;
class MojoVideoEncoderMetricsProviderFactory;
}

namespace gfx {
class ColorSpace;
}

namespace blink {

PLATFORM_EXPORT std::unique_ptr<webrtc::VideoEncoderFactory>
CreateWebrtcVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
        encoder_metrics_provider_factory,
    StatsCollector::StoreProcessingStatsCB stats_callback);
PLATFORM_EXPORT std::unique_ptr<webrtc::VideoDecoderFactory>
CreateWebrtcVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    const gfx::ColorSpace& render_color_space,
    StatsCollector::StoreProcessingStatsCB stats_callback);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_VIDEO_CODEC_FACTORY_H_
