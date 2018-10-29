// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_FACTORY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "build/buildflag.h"
#include "media/base/renderer_factory_selector.h"
#include "media/base/routing_token_callback.h"
#include "media/blink/url_index.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/interfaces/remoting.mojom.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_set_sink_id_callbacks.h"
#include "third_party/blink/public/platform/web_string.h"

#if BUILDFLAG(ENABLE_MOJO_MEDIA)
#include "media/mojo/interfaces/interface_factory.mojom.h"  // nogncheck
#endif

namespace blink {
class WebContentDecryptionModule;
class WebEncryptedMediaClient;
class WebLayerTreeView;
class WebLocalFrame;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
}

namespace cc {
class LayerTreeSettings;
}

namespace media {
class CdmFactory;
class DecoderFactory;
class MediaLog;
class MediaObserver;
class RemotePlaybackClientWrapper;
class RendererWebMediaPlayerDelegate;
class WebEncryptedMediaClientImpl;
#if defined(OS_ANDROID)
class RendererMediaPlayerManager;
#endif
}

namespace service_manager {
class InterfaceProvider;
namespace mojom {
class InterfaceProvider;
}
}

namespace content {

class RenderFrameImpl;
class MediaInterfaceFactory;
class MediaStreamRendererFactory;

#if defined(OS_ANDROID)
class RendererMediaPlayerManager;
#endif

// Assist to RenderFrameImpl in creating various media clients.
class MediaFactory {
 public:
  // Helper function returning whether VideoSurfaceLayer should be enabled.
  static blink::WebMediaPlayer::SurfaceLayerMode GetVideoSurfaceLayerMode();

  // Helper function returning whether VideoSurfaceLayer should be enabled for
  // MediaStreams.
  static bool VideoSurfaceLayerEnabledForMS();

  // Create a MediaFactory to assist the |render_frame| with media tasks.
  // |request_routing_token_cb| bound to |render_frame| IPC functions for
  // obtaining overlay tokens.
  MediaFactory(RenderFrameImpl* render_frame,
               media::RequestRoutingTokenCallback request_routing_token_cb);
  ~MediaFactory();

  // Instruct MediaFactory to establish Mojo channels as needed to perform its
  // factory duties. This should be called by RenderFrameImpl as soon as its own
  // interface provider is bound.
  void SetupMojo();

  // Creates the VideoFrameSubmitter and its task_runner based on the current
  // SurfaceLayerMode;
  std::unique_ptr<blink::WebVideoFrameSubmitter> CreateSubmitter(
      scoped_refptr<base::SingleThreadTaskRunner>*
          video_frame_compositor_task_runner,
      const cc::LayerTreeSettings& settings);

  // Creates a new WebMediaPlayer for the given |source| (either a stream or
  // URL). All pointers other than |initial_cdm| are required to be non-null.
  // The created player serves and is directed by the |client| (e.g.
  // HTMLMediaElement). The |encrypted_client| will be used to establish
  // means of decryption for encrypted content. |initial_cdm| should point
  // to a ContentDecryptionModule if MediaKeys have been provided to the
  // |encrypted_client| (otherwise null). |sink_id|, when not empty, identifies
  // the audio sink to use for this player (see HTMLMediaElement.sinkId).
  // The |layer_tree_view| will be used to generate the correct FrameSinkId for
  // the Surface containing the corresponding HTMLMediaElement.
  blink::WebMediaPlayer* CreateMediaPlayer(
      const blink::WebMediaPlayerSource& source,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebContentDecryptionModule* initial_cdm,
      const blink::WebString& sink_id,
      blink::WebLayerTreeView* layer_tree_view,
      const cc::LayerTreeSettings& settings);

  // Provides an EncryptedMediaClient to connect blink's EME layer to media's
  // implementation of requestMediaKeySystemAccess. Will always return the same
  // client whose lifetime is tied to this Factory (same as the RenderFrame).
  blink::WebEncryptedMediaClient* EncryptedMediaClient();

 private:
  std::unique_ptr<media::RendererFactorySelector> CreateRendererFactorySelector(
      media::MediaLog* media_log,
      bool use_media_player,
      media::DecoderFactory* decoder_factory,
      std::unique_ptr<media::RemotePlaybackClientWrapper> client_wrapper,
      base::WeakPtr<media::MediaObserver>* out_media_observer);

  blink::WebMediaPlayer* CreateWebMediaPlayerForMediaStream(
      blink::WebMediaPlayerClient* client,
      const blink::WebString& sink_id,
      const blink::WebSecurityOrigin& security_origin,
      blink::WebLocalFrame* frame,
      blink::WebLayerTreeView* layer_tree_view,
      const cc::LayerTreeSettings& settings);

  // Returns the media delegate for WebMediaPlayer usage.  If
  // |media_player_delegate_| is NULL, one is created.
  media::RendererWebMediaPlayerDelegate* GetWebMediaPlayerDelegate();

  // Creates a MediaStreamRendererFactory used for creating audio and video
  // renderers for WebMediaPlayerMS.
  std::unique_ptr<MediaStreamRendererFactory>
  CreateMediaStreamRendererFactory();

  media::DecoderFactory* GetDecoderFactory();

#if defined(OS_ANDROID)
  RendererMediaPlayerManager* GetMediaPlayerManager();
#endif

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  media::mojom::RemoterFactory* GetRemoterFactory();
#endif

  media::CdmFactory* GetCdmFactory();

#if BUILDFLAG(ENABLE_MOJO_MEDIA)
  media::mojom::InterfaceFactory* GetMediaInterfaceFactory();

  // The media interface provider attached to this frame, lazily initialized.
  std::unique_ptr<MediaInterfaceFactory> media_interface_factory_;
#endif

  // The render frame we're helping. RenderFrameImpl owns this factory, so the
  // pointer will always be valid.
  RenderFrameImpl* render_frame_;

  // Injected callback for requesting overlay routing tokens.
  media::RequestRoutingTokenCallback request_routing_token_cb_;

  // Handy pointer to RenderFrame's remote interfaces. Null until SetupMojo().
  // Lifetime matches that of the owning |render_frame_|. Will always be valid
  // once assigned.
  service_manager::InterfaceProvider* remote_interfaces_ = nullptr;

#if defined(OS_ANDROID)
  // Manages media players and sessions in this render frame for communicating
  // with the real media player and sessions in the browser process.
  // Lifetime is tied to the RenderFrame via the RenderFrameObserver interface.
  // NOTE: This currently only being used in the case where we are casting. See
  // also WebMediaPlayerCast (renderer side) and RemoteMediaPlayerManager
  // (browser side).
  RendererMediaPlayerManager* media_player_manager_ = nullptr;
#endif

  // Manages play, pause notifications for WebMediaPlayer implementations; its
  // lifetime is tied to the RenderFrame via the RenderFrameObserver interface.
  media::RendererWebMediaPlayerDelegate* media_player_delegate_ = nullptr;

  // The CDM and decoder factory attached to this frame, lazily initialized.
  std::unique_ptr<media::DecoderFactory> decoder_factory_;
  std::unique_ptr<media::CdmFactory> cdm_factory_;

  // Media resource cache, lazily initialized.
  std::unique_ptr<media::ResourceFetchContext> fetch_context_;
  std::unique_ptr<media::UrlIndex> url_index_;

  // EncryptedMediaClient attached to this frame; lazily initialized.
  std::unique_ptr<media::WebEncryptedMediaClientImpl>
      web_encrypted_media_client_;

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  // Lazy-bound pointer to the RemoterFactory service in the browser
  // process. Always use the GetRemoterFactory() accessor instead of this.
  media::mojom::RemoterFactoryPtr remoter_factory_;
#endif
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_FACTORY_H_
