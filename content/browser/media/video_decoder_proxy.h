// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_VIDEO_DECODER_PROXY_H_
#define CONTENT_BROWSER_MEDIA_VIDEO_DECODER_PROXY_H_

#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
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
// RenderProcessHostImpl. Unlike MediaInterfaceProxy, only
// CreateVideoDecoder() is implemented. This allows WebRTC to create
// MojoVideoDecoder instances without a RenderFrame.
class CONTENT_EXPORT VideoDecoderProxy : public media::mojom::InterfaceFactory {
 public:
  VideoDecoderProxy();
  ~VideoDecoderProxy() final;

  void Add(mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver);

  // media::mojom::InterfaceFactory implementation.
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) final;
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver) final;
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) final;
#endif
#if defined(OS_ANDROID)
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
#endif  // defined(OS_ANDROID)
  void CreateCdm(const std::string& key_system,
                 mojo::PendingReceiver<media::mojom::ContentDecryptionModule>
                     receiver) final;
  void CreateDecryptor(
      int cdm_id,
      mojo::PendingReceiver<media::mojom::Decryptor> receiver) final;
#if BUILDFLAG(ENABLE_CDM_PROXY)
  void CreateCdmProxy(
      const base::Token& cdm_guid,
      mojo::PendingReceiver<media::mojom::CdmProxy> receiver) final;
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

 private:
  media::mojom::InterfaceFactory* GetMediaInterfaceFactory();
  void ConnectToMediaService();
  void OnMediaServiceConnectionError();

  // Connection to the remote media InterfaceFactory.
  mojo::Remote<media::mojom::InterfaceFactory> interface_factory_remote_;

  // Connections to the renderer.
  mojo::ReceiverSet<media::mojom::InterfaceFactory> receivers_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(VideoDecoderProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_VIDEO_DECODER_PROXY_H_
