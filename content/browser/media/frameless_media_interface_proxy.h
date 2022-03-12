// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_FRAMELESS_MEDIA_INTERFACE_PROXY_H_
#define CONTENT_BROWSER_MEDIA_FRAMELESS_MEDIA_INTERFACE_PROXY_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// This implements the media::mojom::InterfaceFactory interface for a
// RenderProcessHostImpl. It does not support creating services that require a
// frame context (ie. CDMs and renderers).
// It is used in cases without a frame context, e.g. WebRTC's
// RTCVideoDecoderFactory to create hardware video decoders using
// MojoVideoDecoder, and WebCodecs audio/video decoding in workers.
class FramelessMediaInterfaceProxy final
    : public media::mojom::InterfaceFactory {
 public:
  FramelessMediaInterfaceProxy();

  FramelessMediaInterfaceProxy(const FramelessMediaInterfaceProxy&) = delete;
  FramelessMediaInterfaceProxy& operator=(const FramelessMediaInterfaceProxy&) =
      delete;

  ~FramelessMediaInterfaceProxy() final;

  void Add(mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver);

  // media::mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) final;
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver) final;
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
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
          renderer_extension_receiver) final;
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
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
  void ConnectToMediaService();
  void OnMediaServiceConnectionError();

  // Connection to the remote media InterfaceFactory.
  mojo::Remote<media::mojom::InterfaceFactory> interface_factory_remote_;

  // Connections to the renderer.
  mojo::ReceiverSet<media::mojom::InterfaceFactory> receivers_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_FRAMELESS_MEDIA_INTERFACE_PROXY_H_
