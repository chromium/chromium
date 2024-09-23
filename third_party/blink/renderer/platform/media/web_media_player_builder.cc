// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/media/web_media_player_builder.h"

#include <utility>

#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/demuxer.h"
#include "media/base/media_log.h"
#include "media/base/media_observer.h"
#include "media/base/renderer_factory_selector.h"
#include "media/mojo/mojom/media_metrics_provider.mojom.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/media/video_frame_compositor.h"
#include "third_party/blink/public/platform/media/web_media_player_delegate.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/platform/media/resource_fetch_context.h"
#include "third_party/blink/renderer/platform/media/url_index.h"
#include "third_party/blink/renderer/platform/media/web_media_player_impl.h"

namespace blink {

namespace {

class FrameFetchContext : public ResourceFetchContext {
 public:
  explicit FrameFetchContext(WebLocalFrame& frame) : frame_(frame) {}
  FrameFetchContext(const FrameFetchContext&) = delete;
  FrameFetchContext& operator=(const FrameFetchContext&) = delete;
  ~FrameFetchContext() override = default;

  WebLocalFrame& frame() const { return *frame_; }

  // ResourceFetchContext:
  std::unique_ptr<WebAssociatedURLLoader> CreateUrlLoader(
      const WebAssociatedURLLoaderOptions& options) override {
    return frame_->CreateAssociatedURLLoader(options);
  }

 private:
  const raw_ref<WebLocalFrame> frame_;
};

}  // namespace

WebMediaPlayerBuilder::WebMediaPlayerBuilder(
    WebLocalFrame& frame,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : fetch_context_(std::make_unique<FrameFetchContext>(frame)),
      url_index_(std::make_unique<UrlIndex>(fetch_context_.get(),
                                            std::move(task_runner))) {}

WebMediaPlayerBuilder::~WebMediaPlayerBuilder() = default;

std::unique_ptr<WebMediaPlayer> WebMediaPlayerBuilder::Build(
    WebLocalFrame* frame,
    WebMediaPlayerClient* client,
    WebMediaPlayerEncryptedMediaClient* encrypted_client,
    WebMediaPlayerDelegate* delegate,
    std::unique_ptr<media::RendererFactorySelector> factory_selector,
    std::unique_ptr<VideoFrameCompositor> compositor,
    std::unique_ptr<media::MediaLog> media_log,
    media::MediaPlayerLoggingID player_id,
    DeferLoadCB defer_load_cb,
    scoped_refptr<media::SwitchableAudioRendererSink> audio_renderer_sink,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    scoped_refptr<base::TaskRunner> worker_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner>
        video_frame_compositor_task_runner,
    WebContentDecryptionModule* initial_cdm,
    media::RequestRoutingTokenCallback request_routing_token_cb,
    base::WeakPtr<media::MediaObserver> media_observer,
    bool enable_instant_source_buffer_gc,
    bool embedded_media_experience_enabled,
    mojo::PendingRemote<media::mojom::MediaMetricsProvider> metrics_provider,
    CreateSurfaceLayerBridgeCB create_bridge_callback,
    scoped_refptr<viz::RasterContextProvider> raster_context_provider,
    bool use_surface_layer,
    bool is_background_suspend_enabled,
    bool is_background_video_playback_enabled,
    bool is_background_video_track_optimization_supported,
    std::unique_ptr<media::Demuxer> demuxer_override,
    scoped_refptr<ThreadSafeBrowserInterfaceBrokerProxy> remote_interfaces) {
  DCHECK_EQ(&static_cast<FrameFetchContext*>(fetch_context_.get())->frame(),
            frame);
  return std::make_unique<WebMediaPlayerImpl>(
      frame, client, encrypted_client, delegate, std::move(factory_selector),
      url_index_.get(), std::move(compositor), std::move(media_log), player_id,
      std::move(defer_load_cb), std::move(audio_renderer_sink),
      std::move(media_task_runner), std::move(worker_task_runner),
      std::move(compositor_task_runner),
      std::move(video_frame_compositor_task_runner), initial_cdm,
      std::move(request_routing_token_cb), std::move(media_observer),
      enable_instant_source_buffer_gc, embedded_media_experience_enabled,
      std::move(metrics_provider), std::move(create_bridge_callback),
      std::move(raster_context_provider), use_surface_layer,
      is_background_suspend_enabled, is_background_video_playback_enabled,
      is_background_video_track_optimization_supported,
      std::move(demuxer_override), std::move(remote_interfaces));
}

}  // namespace blink
