// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/media/web_media_player_builder.h"

#include <utility>

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/demuxer.h"
#include "media/base/media_log.h"
#include "media/base/media_observer.h"
#include "media/base/renderer_factory_selector.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-shared.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/media/web_media_player_delegate.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_video_frame_submitter.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/renderer/platform/media/media_player_client.h"
#include "third_party/blink/renderer/platform/media/resource_fetch_context.h"
#include "third_party/blink/renderer/platform/media/url_index.h"
#include "third_party/blink/renderer/platform/media/video_frame_compositor.h"
#include "third_party/blink/renderer/platform/media/web_media_player_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace blink {

namespace {

class UrlLoaderFetchContext : public ResourceFetchContext {
 public:
  explicit UrlLoaderFetchContext(
      WebMediaPlayerBuilder::CreateUrlLoaderCB create_url_loader_cb)
      : create_url_loader_cb_(std::move(create_url_loader_cb)) {}
  UrlLoaderFetchContext(const UrlLoaderFetchContext&) = delete;
  UrlLoaderFetchContext& operator=(const UrlLoaderFetchContext&) = delete;
  ~UrlLoaderFetchContext() override = default;

  // ResourceFetchContext:
  std::unique_ptr<WebAssociatedURLLoader> CreateUrlLoader(
      const WebAssociatedURLLoaderOptions& options) override {
    return create_url_loader_cb_.Run(options);
  }

 private:
  const WebMediaPlayerBuilder::CreateUrlLoaderCB create_url_loader_cb_;
};

}  // namespace

WebMediaPlayerBuilder::WebMediaPlayerBuilder(
    FrameScheduler& frame_scheduler,
    media::RequestRoutingTokenCallback request_routing_token_cb,
    HasTransientUserActivationCB has_transient_user_activation_cb,
    CreateUrlLoaderCB create_url_loader_cb)
    : frame_scheduler_(frame_scheduler.GetWeakPtr()),
      request_routing_token_cb_(std::move(request_routing_token_cb)),
      has_transient_user_activation_cb_(
          std::move(has_transient_user_activation_cb)),
      fetch_context_(std::make_unique<UrlLoaderFetchContext>(
          std::move(create_url_loader_cb))),
      url_index_(std::make_unique<UrlIndex>(
          fetch_context_.get(),
          frame_scheduler.GetTaskRunner(TaskType::kNetworkingUnfreezable))) {}

WebMediaPlayerBuilder::~WebMediaPlayerBuilder() = default;

std::unique_ptr<WebMediaPlayer> WebMediaPlayerBuilder::Build(
    const WebSecurityOrigin& security_origin,
    const WebURL& document_url,
    const WebString& document_title,
    WebMediaPlayerClient* client,
    WebMediaPlayerEncryptedMediaClient* encrypted_client,
    WebMediaPlayerDelegate* delegate,
    std::unique_ptr<media::RendererFactorySelector> factory_selector,
    std::unique_ptr<WebVideoFrameSubmitter> video_frame_submitter,
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
    base::WeakPtr<media::MediaObserver> media_observer,
    bool embedded_media_experience_enabled,
    CrossVariantMojoRemote<media::mojom::MediaMetricsProviderInterfaceBase>
        metrics_provider,
    CreateSurfaceLayerBridgeCB create_bridge_callback,
    scoped_refptr<viz::RasterContextProvider> raster_context_provider,
    bool is_background_suspend_enabled,
    bool is_background_video_playback_enabled,
    bool is_background_video_track_optimization_supported,
    std::unique_ptr<media::Demuxer> demuxer_override,
    scoped_refptr<ThreadSafeBrowserInterfaceBrokerProxy> remote_interfaces) {
  CHECK(frame_scheduler_);
  auto video_frame_compositor = std::make_unique<VideoFrameCompositor>(
      video_frame_compositor_task_runner, std::move(video_frame_submitter));
  return std::make_unique<WebMediaPlayerImpl>(
      frame_scheduler_.get(), security_origin, document_url, document_title,
      has_transient_user_activation_cb_,
      static_cast<MediaPlayerClient*>(client), encrypted_client, delegate,
      std::move(factory_selector), url_index_.get(),
      std::move(video_frame_compositor), std::move(media_log), player_id,
      std::move(defer_load_cb), std::move(audio_renderer_sink),
      std::move(media_task_runner), std::move(worker_task_runner),
      std::move(compositor_task_runner),
      std::move(video_frame_compositor_task_runner), initial_cdm,
      request_routing_token_cb_, std::move(media_observer),
      embedded_media_experience_enabled, std::move(metrics_provider),
      std::move(create_bridge_callback), std::move(raster_context_provider),
      is_background_suspend_enabled, is_background_video_playback_enabled,
      is_background_video_track_optimization_supported,
      std::move(demuxer_override), std::move(remote_interfaces));
}

}  // namespace blink
