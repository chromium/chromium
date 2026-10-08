// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_WEB_MEDIA_PLAYER_BUILDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_WEB_MEDIA_PLAYER_BUILDER_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/layers/surface_layer.h"
#include "media/base/media_player_logging_id.h"
#include "media/base/routing_token_callback.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_player.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
class TaskRunner;
}  // namespace base

namespace media {
class Demuxer;
class MediaLog;
class MediaObserver;
class RendererFactorySelector;
class SwitchableAudioRendererSink;
}  // namespace media

namespace viz {
class RasterContextProvider;
}

namespace blink {

class FrameScheduler;
class ResourceFetchContext;
class ThreadSafeBrowserInterfaceBrokerProxy;
class UrlIndex;
class WebAssociatedURLLoader;
class WebContentDecryptionModule;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
class WebMediaPlayerDelegate;
class WebSecurityOrigin;
class WebString;
class WebSurfaceLayerBridge;
class WebSurfaceLayerBridgeObserver;
class WebURL;
class WebVideoFrameSubmitter;
struct WebAssociatedURLLoaderOptions;

using CreateSurfaceLayerBridgeCB =
    base::OnceCallback<std::unique_ptr<WebSurfaceLayerBridge>(
        WebSurfaceLayerBridgeObserver*,
        cc::UpdateSubmissionStateCB)>;

class BLINK_PLATFORM_EXPORT WebMediaPlayerBuilder {
 public:
  // Returns true if load will deferred. False if it will run immediately.
  using DeferLoadCB = base::RepeatingCallback<bool(base::OnceClosure)>;

  // Creates the URL loaders used to fetch media resources.
  using CreateUrlLoaderCB =
      base::RepeatingCallback<std::unique_ptr<WebAssociatedURLLoader>(
          const WebAssociatedURLLoaderOptions&)>;

  // Returns whether the user recently interacted with the frame the player
  // belongs to.
  using HasTransientUserActivationCB = base::RepeatingCallback<bool()>;

  WebMediaPlayerBuilder(
      FrameScheduler& frame_scheduler,
      media::RequestRoutingTokenCallback request_routing_token_cb,
      HasTransientUserActivationCB has_transient_user_activation_cb,
      CreateUrlLoaderCB create_url_loader_cb);
  WebMediaPlayerBuilder(const WebMediaPlayerBuilder&) = delete;
  WebMediaPlayerBuilder& operator=(const WebMediaPlayerBuilder&) = delete;
  ~WebMediaPlayerBuilder();

  std::unique_ptr<WebMediaPlayer> Build(
      const WebSecurityOrigin& security_origin,
      const WebURL& document_url,
      const WebString& document_title,
      WebMediaPlayerClient* client,
      WebMediaPlayerEncryptedMediaClient* encrypted_client,
      WebMediaPlayerDelegate* delegate,
      std::unique_ptr<media::RendererFactorySelector> renderer_factory_selector,
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
      scoped_refptr<ThreadSafeBrowserInterfaceBrokerProxy> remote_interfaces);

 private:
  // These belong to the frame all the players are built for.
  const base::WeakPtr<FrameScheduler> frame_scheduler_;
  const media::RequestRoutingTokenCallback request_routing_token_cb_;
  const HasTransientUserActivationCB has_transient_user_activation_cb_;

  // Media resource cache.
  std::unique_ptr<ResourceFetchContext> fetch_context_;
  std::unique_ptr<UrlIndex> url_index_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_WEB_MEDIA_PLAYER_BUILDER_H_
