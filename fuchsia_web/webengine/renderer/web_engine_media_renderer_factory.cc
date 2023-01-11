// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/renderer/web_engine_media_renderer_factory.h"

#include <fuchsia/media/cpp/fidl.h>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "fuchsia_web/webengine/renderer/web_engine_audio_renderer.h"
#include "media/base/decoder_factory.h"
#include "media/renderers/renderer_impl.h"
#include "media/renderers/video_renderer_impl.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "media/video/gpu_video_accelerator_factories.h"

WebEngineMediaRendererFactory::WebEngineMediaRendererFactory(
    media::MediaLog* media_log,
    media::DecoderFactory* decoder_factory,
    GetGpuFactoriesCB get_gpu_factories_cb,
    mojo::Remote<mojom::WebEngineMediaResourceProvider> media_resource_provider)
    : media_log_(media_log),
      decoder_factory_(decoder_factory),
      get_gpu_factories_cb_(std::move(get_gpu_factories_cb)),
      media_resource_provider_(std::move(media_resource_provider)) {
  DCHECK(decoder_factory_);
}

WebEngineMediaRendererFactory::~WebEngineMediaRendererFactory() = default;

std::vector<std::unique_ptr<media::VideoDecoder>>
WebEngineMediaRendererFactory::CreateVideoDecoders(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  std::vector<std::unique_ptr<media::VideoDecoder>> video_decoders;
  decoder_factory_->CreateVideoDecoders(
      media_task_runner, gpu_factories, media_log_,
      std::move(request_overlay_info_cb), target_color_space, &video_decoders);
  return video_decoders;
}

std::unique_ptr<media::Renderer> WebEngineMediaRendererFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::AudioRendererSink* audio_renderer_sink,
    media::VideoRendererSink* video_renderer_sink,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer_handle;
  media_resource_provider_->CreateAudioConsumer(
      audio_consumer_handle.NewRequest());
  auto audio_renderer = std::make_unique<WebEngineAudioRenderer>(
      media_log_, std::move(audio_consumer_handle));

  media::GpuVideoAcceleratorFactories* gpu_factories = nullptr;
  if (get_gpu_factories_cb_)
    gpu_factories = get_gpu_factories_cb_.Run();

  std::unique_ptr<media::GpuMemoryBufferVideoFramePool> gmb_pool;
  if (gpu_factories && gpu_factories->ShouldUseGpuMemoryBuffersForVideoFrames(
                           /*for_media_stream=*/false)) {
    gmb_pool = std::make_unique<media::GpuMemoryBufferVideoFramePool>(
        media_task_runner, std::move(worker_task_runner), gpu_factories);
  }

  std::unique_ptr<media::VideoRenderer> video_renderer(
      new media::VideoRendererImpl(
          media_task_runner, video_renderer_sink,
          // Unretained is safe here, because the RendererFactory is guaranteed
          // to outlive the RendererImpl. The RendererImpl is destroyed when
          // WMPI destructor calls pipeline_controller_.Stop() ->
          // PipelineImpl::Stop() -> RendererWrapper::Stop ->
          // RendererWrapper::DestroyRenderer(). And the RendererFactory is
          // owned by WMPI and gets called after WMPI destructor finishes.
          base::BindRepeating(
              &WebEngineMediaRendererFactory::CreateVideoDecoders,
              base::Unretained(this), media_task_runner,
              std::move(request_overlay_info_cb), target_color_space,
              gpu_factories),
          /*drop_frames=*/true, media_log_, std::move(gmb_pool),
          media::GetNextMediaPlayerLoggingID()));

  return std::make_unique<media::RendererImpl>(
      media_task_runner, std::move(audio_renderer), std::move(video_renderer));
}
