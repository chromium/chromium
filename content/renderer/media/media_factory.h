// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromecast_buildflags.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "media/base/key_system_info.h"
#include "media/base/key_systems.h"
#include "media/base/key_systems_support_registration.h"
#include "media/base/media_player_logging_id.h"
#include "media/base/renderer_factory_selector.h"
#include "media/base/routing_token_callback.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/clients/mojo_renderer_factory.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_set_sink_id_callbacks.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_media_inspector.h"

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
// Needed by remoting sender.
#include "media/mojo/mojom/remoting.mojom.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)

namespace blink {
class BrowserInterfaceBrokerProxy;
class WebContentDecryptionModule;
class WebEncryptedMediaClient;
class WebEncryptedMediaClientImpl;
class WebLocalFrame;
class WebMediaPlayer;
class WebMediaPlayerBuilder;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
}  // namespace blink

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
namespace cast_streaming {
class ResourceProvider;
}  // namespace cast_streaming
#endif

namespace cc {
class LayerTreeSettings;
}  // namespace cc

namespace media {
class CdmFactory;
class DecoderFactory;
class DefaultDecoderFactory;
class MediaLog;
class MediaObserver;
class RemotePlaybackClientWrapper;
class RendererWebMediaPlayerDelegate;
}  // namespace media

namespace content {

class RenderFrameImpl;
class MediaInterfaceFactory;
struct RenderFrameMediaPlaybackOptions;

// Assist to RenderFrameImpl in creating various media clients.
class MediaFactory {
 public:
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

  // Creates a new WebMediaPlayer for the given |source| (either a stream or
  // URL). All pointers other than |initial_cdm| are required to be non-null.
  // The created player serves and is directed by the |client| (e.g.
  // HTMLMediaElement). The |encrypted_client| will be used to establish
  // means of decryption for encrypted content. |initial_cdm| should point
  // to a ContentDecryptionModule if MediaKeys have been provided to the
  // |encrypted_client| (otherwise null). |sink_id|, when not empty, identifies
  // the audio sink to use for this player (see HTMLMediaElement.sinkId).
  // |parent_frame_sink_id| identifies the local root widget's FrameSinkId.
  std::unique_ptr<blink::WebMediaPlayer> CreateMediaPlayer(
      const blink::WebMediaPlayerSource& source,
      blink::WebMediaPlayerClient* client,
      blink::MediaInspectorContext* inspector_context,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      blink::WebContentDecryptionModule* initial_cdm,
      const blink::WebString& sink_id,
      viz::FrameSinkId parent_frame_sink_id,
      const cc::LayerTreeSettings& settings,
      scoped_refptr<base::SingleThreadTaskRunner>
          main_thread_compositor_task_runner,
      scoped_refptr<base::TaskRunner> compositor_worker_task_runner);

  // Provides an EncryptedMediaClient to connect blink's EME layer to media's
  // implementation of requestMediaKeySystemAccess. Will always return the same
  // client whose lifetime is tied to this Factory (same as the RenderFrame).
  blink::WebEncryptedMediaClient* EncryptedMediaClient();

  // Returns `DecoderFactory`, which can be used to created decoders in WebRTC.
  // Can be dereferenced only on the media thread.
  base::WeakPtr<media::DecoderFactory> GetDecoderFactory();

 private:
  // Initializes `decoder_factory_` if it hasn't been initialized yet.
  void EnsureDecoderFactory();

  std::unique_ptr<media::RendererFactorySelector> CreateRendererFactorySelector(
      media::MediaPlayerLoggingID player_id,
      media::MediaLog* media_log,
      blink::WebURL url,
      const RenderFrameMediaPlaybackOptions& renderer_media_playback_options,
      media::DecoderFactory* decoder_factory,
      std::unique_ptr<media::RemotePlaybackClientWrapper> client_wrapper,
      base::WeakPtr<media::MediaObserver>* out_media_observer,
      int element_id);

  std::unique_ptr<blink::WebMediaPlayer> CreateWebMediaPlayerForMediaStream(
      blink::WebMediaPlayerClient* client,
      blink::MediaInspectorContext* inspector_context,
      const blink::WebString& sink_id,
      blink::WebLocalFrame* frame,
      viz::FrameSinkId parent_frame_sink_id,
      const cc::LayerTreeSettings& settings,
      scoped_refptr<base::SingleThreadTaskRunner>
          main_thread_compositor_task_runner,
      scoped_refptr<base::TaskRunner> compositor_worker_task_runner);

  // Returns the media delegate for WebMediaPlayer usage.  If
  // |media_player_delegate_| is NULL, one is created.
  media::RendererWebMediaPlayerDelegate* GetWebMediaPlayerDelegate();

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  media::mojom::RemoterFactory* GetRemoterFactory();
#endif

  // Initializes the key systems remote and receivers.
  std::unique_ptr<media::KeySystemSupportRegistration> GetSupportedKeySystems(
      media::GetSupportedKeySystemsCB cb);

  media::KeySystems* GetKeySystems();

  media::CdmFactory* GetCdmFactory();

  media::mojom::InterfaceFactory* GetMediaInterfaceFactory();

  std::unique_ptr<media::MojoRendererFactory> CreateMojoRendererFactory();

  const blink::BrowserInterfaceBrokerProxy& GetInterfaceBroker() const;

  // The render frame we're helping. RenderFrameImpl owns this factory, so the
  // pointer will always be valid.
  raw_ptr<RenderFrameImpl> render_frame_;

  // The media interface provider attached to this frame, lazily initialized.
  std::unique_ptr<MediaInterfaceFactory> media_interface_factory_;

  // Injected callback for requesting overlay routing tokens.
  media::RequestRoutingTokenCallback request_routing_token_cb_;

  // Manages play, pause notifications for WebMediaPlayer implementations; its
  // lifetime is tied to the RenderFrame via the RenderFrameObserver interface.
  raw_ptr<media::RendererWebMediaPlayerDelegate, DanglingUntriaged>
      media_player_delegate_ = nullptr;

  // The `KeySystems` to be used by `web_encrypted_media_client_`. This object
  // must outlive `web_encrypted_media_client_` and `cdm_factory_` since they
  // reference it.
  std::unique_ptr<media::KeySystems> key_systems_;

  // The CDM and decoder factory attached to this frame, lazily initialized.
  std::unique_ptr<media::DefaultDecoderFactory> decoder_factory_;
  std::unique_ptr<media::CdmFactory> cdm_factory_;

  // `WebMediaPlayer` builder, that acts as a media resource cache, lazily
  // initialized.
  std::unique_ptr<blink::WebMediaPlayerBuilder> media_player_builder_;

  // EncryptedMediaClient attached to this frame; lazily initialized.
  std::unique_ptr<blink::WebEncryptedMediaClientImpl>
      web_encrypted_media_client_;

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  // Lazy-bound remote for the RemoterFactory service in the browser
  // process. Always use the GetRemoterFactory() accessor instead of this.
  mojo::Remote<media::mojom::RemoterFactory> remoter_factory_;
#endif

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  std::unique_ptr<cast_streaming::ResourceProvider>
      cast_streaming_resource_provider_;
#endif
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_FACTORY_H_
