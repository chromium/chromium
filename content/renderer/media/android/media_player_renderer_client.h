// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_CLIENT_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_CLIENT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "media/base/android/stream_texture_wrapper.h"
#include "media/base/media_resource.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/clients/mojo_renderer_wrapper.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// MediaPlayerRendererClient lives in Renderer process and mirrors a
// MediaPlayerRenderer living in the Browser process.
//
// It is primarily used as a media::Renderer that forwards calls from WMPI to
// the MediaPlayerRenderer, by inheriting from MojoRendererWrapper.
// It also manages a StreamTexture, and notifies the VideoRendererSink when new
// frames are available.
//
// This class handles all calls on |media_task_runner_|, except for
// OnFrameAvailable(), which is called on |compositor_task_runner_|.
class CONTENT_EXPORT MediaPlayerRendererClient
    : public media::mojom::MediaPlayerRendererClientExtension,
      public media::MojoRendererWrapper {
 public:
  using RendererExtention = media::mojom::MediaPlayerRendererExtension;
  using ClientExtention = media::mojom::MediaPlayerRendererClientExtension;

  MediaPlayerRendererClient(
      mojo::PendingRemote<RendererExtention> renderer_extension_remote,
      mojo::PendingReceiver<ClientExtention> client_extension_receiver,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      std::unique_ptr<media::MojoRenderer> mojo_renderer,
      media::ScopedStreamTextureWrapper stream_texture_wrapper,
      media::VideoRendererSink* sink);

  ~MediaPlayerRendererClient() override;

  // media::Renderer implementation (inherited from media::MojoRendererWrapper).
  // We override normal initialization to set up |stream_texture_wrapper_|,
  // and do not support encrypted media.
  void Initialize(media::MediaResource* media_resource,
                  media::RendererClient* client,
                  media::PipelineStatusCallback init_cb) override;
  void SetCdm(media::CdmContext* cdm_context,
              media::CdmAttachedCB cdm_attached_cb) override;

  // media::mojom::MediaPlayerRendererClientExtension implementation
  void OnDurationChange(base::TimeDelta duration) override;
  void OnVideoSizeChange(const gfx::Size& size) override;

  // Called on |compositor_task_runner_| whenever |stream_texture_wrapper_| has
  // a new frame.
  void OnFrameAvailable();

 private:
  void OnStreamTextureWrapperInitialized(media::MediaResource* media_resource,
                                         bool success);
  void OnRemoteRendererInitialized(media::PipelineStatus status);

  void OnScopedSurfaceRequested(const base::UnguessableToken& request_token);

  // The underlying type should always be a MediaUrlDemuxer, but we only use
  // methods from the MediaResource interface.
  media::MediaResource* media_resource_;

  // Owns the StreamTexture whose surface is used by MediaPlayerRenderer.
  // Provides the VideoFrames to |sink_|.
  media::ScopedStreamTextureWrapper stream_texture_wrapper_;

  media::RendererClient* client_;

  media::VideoRendererSink* sink_;

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // Used by |stream_texture_wrapper_| to signal OnFrameAvailable() and to send
  // VideoFrames to |sink_| on the right thread.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  media::PipelineStatusCallback init_cb_;

  // This class is constructed on the main task runner, and used on
  // |media_task_runner_|. These member are used to delay calls to Bind() for
  // |renderer_extension_ptr_| and |client_extension_binding_|, until we are on
  // |media_task_runner_|.
  // Both are set in the constructor, and consumed in Initialize().
  mojo::PendingReceiver<ClientExtention>
      delayed_bind_client_extension_receiver_;
  mojo::PendingRemote<RendererExtention>
      delayed_bind_renderer_extention_remote_;

  // Used to call methods on the MediaPlayerRenderer in the browser process.
  mojo::Remote<RendererExtention> renderer_extension_remote_;

  // Used to receive events from MediaPlayerRenderer in the browser process.
  mojo::Receiver<MediaPlayerRendererClientExtension> client_extension_receiver_{
      this};

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaPlayerRendererClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerRendererClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_MEDIA_PLAYER_RENDERER_CLIENT_H_
