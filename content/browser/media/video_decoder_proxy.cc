// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/video_decoder_proxy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/system_connector.h"
#include "media/mojo/mojom/constants.mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

VideoDecoderProxy::VideoDecoderProxy() {
  DVLOG(1) << __func__;
}

VideoDecoderProxy::~VideoDecoderProxy() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void VideoDecoderProxy::Add(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  receivers_.Add(this, std::move(receiver));
}

void VideoDecoderProxy::CreateAudioDecoder(
    mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) {}

void VideoDecoderProxy::CreateVideoDecoder(
    mojo::PendingReceiver<media::mojom::VideoDecoder> receiver) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateVideoDecoder(std::move(receiver));
}

void VideoDecoderProxy::CreateDefaultRenderer(
    const std::string& audio_device_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void VideoDecoderProxy::CreateCastRenderer(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if defined(OS_ANDROID)
void VideoDecoderProxy::CreateFlingingRenderer(
    const std::string& audio_device_id,
    mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
        client_extenion,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {}

void VideoDecoderProxy::CreateMediaPlayerRenderer(
    mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
        client_extension_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
        renderer_extension_receiver) {}
#endif  // defined(OS_ANDROID)

void VideoDecoderProxy::CreateCdm(
    const std::string& key_system,
    mojo::PendingReceiver<media::mojom::ContentDecryptionModule> receiver) {}

void VideoDecoderProxy::CreateDecryptor(
    int cdm_id,
    mojo::PendingReceiver<media::mojom::Decryptor> receiver) {}

#if BUILDFLAG(ENABLE_CDM_PROXY)
void VideoDecoderProxy::CreateCdmProxy(
    const base::Token& cdm_guid,
    mojo::PendingReceiver<media::mojom::CdmProxy> receiver) {}
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

media::mojom::InterfaceFactory* VideoDecoderProxy::GetMediaInterfaceFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!interface_factory_remote_)
    ConnectToMediaService();

  return interface_factory_remote_.get();
}

void VideoDecoderProxy::ConnectToMediaService() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!interface_factory_remote_);

  media::mojom::MediaServicePtr media_service;
  // TODO(slan): Use the BrowserContext Connector instead.
  // See https://crbug.com/638950.
  GetSystemConnector()->BindInterface(media::mojom::kMediaServiceName,
                                      &media_service);

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider> interfaces;
  ignore_result(interfaces.InitWithNewPipeAndPassReceiver());
  media_service->CreateInterfaceFactory(
      interface_factory_remote_.BindNewPipeAndPassReceiver(),
      std::move(interfaces));

  interface_factory_remote_.set_disconnect_handler(
      base::BindOnce(&VideoDecoderProxy::OnMediaServiceConnectionError,
                     base::Unretained(this)));
}

void VideoDecoderProxy::OnMediaServiceConnectionError() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  interface_factory_remote_.reset();
}

}  // namespace content
