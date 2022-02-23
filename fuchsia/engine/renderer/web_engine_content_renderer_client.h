// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_
#define FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_

#include "components/cast_streaming/renderer/public/demuxer_provider.h"
#include "content/public/renderer/content_renderer_client.h"
#include "fuchsia/engine/renderer/web_engine_audio_device_factory.h"
#include "fuchsia/engine/renderer/web_engine_render_frame_observer.h"

namespace memory_pressure {
class MultiSourceMemoryPressureMonitor;
}  // namespace memory_pressure

class WebEngineContentRendererClient : public content::ContentRendererClient {
 public:
  WebEngineContentRendererClient();

  WebEngineContentRendererClient(const WebEngineContentRendererClient&) =
      delete;
  WebEngineContentRendererClient& operator=(
      const WebEngineContentRendererClient&) = delete;

  ~WebEngineContentRendererClient() override;

  // Returns the WebEngineRenderFrameObserver corresponding to
  // |render_frame_id|.
  WebEngineRenderFrameObserver* GetWebEngineRenderFrameObserverForRenderFrameId(
      int render_frame_id) const;

 private:
  // Called by WebEngineRenderFrameObserver when its corresponding RenderFrame
  // is in the process of being deleted.
  void OnRenderFrameDeleted(int render_frame_id);

  // content::ContentRendererClient overrides.
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) override;
  bool IsSupportedVideoType(const media::VideoType& type) override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType type) override;
  bool DeferMediaLoad(content::RenderFrame* render_frame,
                      bool has_played_media_before,
                      base::OnceClosure closure) override;
  std::unique_ptr<media::Demuxer> OverrideDemuxerForUrl(
      content::RenderFrame* render_frame,
      const GURL& url,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) override;
  std::unique_ptr<media::RendererFactory> GetBaseRendererFactory(
      content::RenderFrame* render_frame,
      media::MediaLog* media_log,
      media::DecoderFactory* decoder_factory,
      base::RepeatingCallback<media::GpuVideoAcceleratorFactories*()>
          get_gpu_factories_cb) override;

  bool RunClosureWhenInForeground(content::RenderFrame* render_frame,
                                  base::OnceClosure closure);

  // Overrides the default Content/Blink audio pipeline, to allow Renderers to
  // use the AudioConsumer service directly.
  WebEngineAudioDeviceFactory audio_device_factory_;

  // Handles interaction with cast_streaming component.
  cast_streaming::DemuxerProvider cast_streaming_demuxer_provider_;

  // Map of RenderFrame ID to WebEngineRenderFrameObserver.
  std::map<int, std::unique_ptr<WebEngineRenderFrameObserver>>
      render_frame_id_to_observer_map_;

  // Initiates cache purges and Blink/V8 garbage collection when free memory
  // is limited.
  std::unique_ptr<memory_pressure::MultiSourceMemoryPressureMonitor>
      memory_pressure_monitor_;
};

#endif  // FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_
