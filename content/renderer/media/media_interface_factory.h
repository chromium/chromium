// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_INTERFACE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_INTERFACE_FACTORY_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace blink {
class BrowserInterfaceBrokerProxy;
}

namespace content {

// MediaInterfaceFactory is an implementation of media::mojom::InterfaceFactory
// that provides thread safety and handles disconnection error automatically.
// The Create* methods can be called on any thread.
class MediaInterfaceFactory final : public media::mojom::InterfaceFactory {
 public:
  explicit MediaInterfaceFactory(
      const blink::BrowserInterfaceBrokerProxy* interface_broker);
  // This ctor is intended for use by the RenderThread, which doesn't have an
  // interface broker.  This is only necessary for WebRTC, and should be avoided
  // if we can restructure WebRTC to create factories per-frame rather than
  // per-process.
  MediaInterfaceFactory(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingRemote<media::mojom::InterfaceFactory> interface_factory);

  MediaInterfaceFactory(const MediaInterfaceFactory&) = delete;
  MediaInterfaceFactory& operator=(const MediaInterfaceFactory&) = delete;

  ~MediaInterfaceFactory() final;

  // media::mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) final;
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
          dst_video_decoder) final;
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
          video_decoder) final;
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateAudioEncoder(
      mojo::PendingReceiver<media::mojom::AudioEncoder> receiver) final;
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#endif
#if BUILDFLAG(IS_ANDROID)
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
          renderer_extension_receiver) final;
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  void CreateMediaFoundationRenderer(
      mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver,
      mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
          client_extension_remote) final;
#endif  // BUILDFLAG(IS_WIN)
  void CreateCdm(const media::CdmConfig& cdm_config,
                 CreateCdmCallback callback) final;

 private:
  media::mojom::InterfaceFactory* GetMediaInterfaceFactory();
  void OnConnectionError();

  raw_ptr<const blink::BrowserInterfaceBrokerProxy> interface_broker_;
  mojo::Remote<media::mojom::InterfaceFactory> media_interface_factory_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<MediaInterfaceFactory> weak_this_;
  base::WeakPtrFactory<MediaInterfaceFactory> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_INTERFACE_FACTORY_H_
