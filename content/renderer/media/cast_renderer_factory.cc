// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cast_renderer_factory.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/audio/cast_audio_renderer.h"
#include "media/base/decoder_factory.h"
#include "media/renderers/renderer_impl.h"
#include "media/renderers/video_renderer_impl.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace content {

CastRendererFactory::CastRendererFactory(
    media::MediaLog* media_log,
    media::DecoderFactory* decoder_factory,
    GetGpuFactoriesCB get_gpu_factories_cb,
    blink::BrowserInterfaceBrokerProxy* interface_broker)
    : media_log_(media_log),
      decoder_factory_(decoder_factory),
      interface_broker_(interface_broker),
      get_gpu_factories_cb_(std::move(get_gpu_factories_cb)) {
  DCHECK(decoder_factory_);
}

CastRendererFactory::~CastRendererFactory() = default;

std::unique_ptr<media::Renderer> CastRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::AudioRendererSink* audio_renderer_sink,
    media::VideoRendererSink* video_renderer_sink,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  auto audio_renderer = std::make_unique<chromecast::media::CastAudioRenderer>(
      media_task_runner, media_log_, interface_broker_);

  // VideoRenderer construction logic is copied from RendererImplFactory.
  media::GpuVideoAcceleratorFactories* gpu_factories = nullptr;
  if (get_gpu_factories_cb_)
    gpu_factories = get_gpu_factories_cb_.Run();

  std::unique_ptr<media::GpuMemoryBufferVideoFramePool> gmb_pool;
  if (gpu_factories && gpu_factories->ShouldUseGpuMemoryBuffersForVideoFrames(
                           false /* for_media_stream */)) {
    gmb_pool = std::make_unique<media::GpuMemoryBufferVideoFramePool>(
        media_task_runner, std::move(worker_task_runner), gpu_factories);
  }

  auto video_renderer = std::make_unique<media::VideoRendererImpl>(
      media_task_runner, video_renderer_sink,
      // Unretained is safe here, because the RendererFactory is guaranteed to
      // outlive the RendererImpl. The RendererImpl is destroyed when WMPI
      // destructor calls pipeline_controller_.Stop() -> PipelineImpl::Stop() ->
      // RendererWrapper::Stop -> RendererWrapper::DestroyRenderer(). And the
      // RendererFactory is owned by WMPI and gets called after WMPI destructor
      // finishes.
      base::BindRepeating(&CastRendererFactory::CreateVideoDecoders,
                          base::Unretained(this), media_task_runner,
                          std::move(request_overlay_info_cb),
                          target_color_space, gpu_factories),
      true, media_log_, std::move(gmb_pool));

  return std::make_unique<media::RendererImpl>(
      media_task_runner, std::move(audio_renderer), std::move(video_renderer));
}

std::vector<std::unique_ptr<media::VideoDecoder>>
CastRendererFactory::CreateVideoDecoders(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  std::vector<std::unique_ptr<media::VideoDecoder>> video_decoders;

  decoder_factory_->CreateVideoDecoders(
      std::move(media_task_runner), gpu_factories, media_log_,
      std::move(request_overlay_info_cb), target_color_space, &video_decoders);

  return video_decoders;
}

}  // namespace content
