// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_interface_factory.h"

#include <string>

#include "base/bind.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

MediaInterfaceFactory::MediaInterfaceFactory(
    service_manager::InterfaceProvider* remote_interfaces)
    : remote_interfaces_(remote_interfaces) {
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  weak_this_ = weak_factory_.GetWeakPtr();
}

MediaInterfaceFactory::~MediaInterfaceFactory() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void MediaInterfaceFactory::CreateAudioDecoder(
    mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaInterfaceFactory::CreateAudioDecoder,
                                  weak_this_, std::move(receiver)));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateAudioDecoder(std::move(receiver));
}

void MediaInterfaceFactory::CreateVideoDecoder(
    mojo::PendingReceiver<media::mojom::VideoDecoder> receiver) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaInterfaceFactory::CreateVideoDecoder,
                                  weak_this_, std::move(receiver)));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateVideoDecoder(std::move(receiver));
}

void MediaInterfaceFactory::CreateDefaultRenderer(
    const std::string& audio_device_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaInterfaceFactory::CreateDefaultRenderer,
                       weak_this_, audio_device_id, std::move(receiver)));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateDefaultRenderer(audio_device_id,
                                                    std::move(receiver));
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void MediaInterfaceFactory::CreateCastRenderer(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaInterfaceFactory::CreateCastRenderer, weak_this_,
                       overlay_plane_id, std::move(receiver)));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateCastRenderer(overlay_plane_id,
                                                 std::move(receiver));
}
#endif

#if defined(OS_ANDROID)
void MediaInterfaceFactory::CreateMediaPlayerRenderer(
    mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
        client_extension_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
        renderer_extension_receiver) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaInterfaceFactory::CreateMediaPlayerRenderer,
                       weak_this_, std::move(client_extension_remote),
                       std::move(receiver),
                       std::move(renderer_extension_receiver)));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateMediaPlayerRenderer(
      std::move(client_extension_remote), std::move(receiver),
      std::move(renderer_extension_receiver));
}

void MediaInterfaceFactory::CreateFlingingRenderer(
    const std::string& presentation_id,
    mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
        client_extension,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaInterfaceFactory::CreateFlingingRenderer,
                       weak_this_, presentation_id, std::move(client_extension),
                       std::move(receiver)));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateFlingingRenderer(
      presentation_id, std::move(client_extension), std::move(receiver));
}
#endif  // defined(OS_ANDROID)

void MediaInterfaceFactory::CreateCdm(
    const std::string& key_system,
    mojo::PendingReceiver<media::mojom::ContentDecryptionModule> receiver) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaInterfaceFactory::CreateCdm, weak_this_,
                                  key_system, std::move(receiver)));
    return;
  }

  DVLOG(1) << __func__ << ": key_system = " << key_system;
  GetMediaInterfaceFactory()->CreateCdm(key_system, std::move(receiver));
}

void MediaInterfaceFactory::CreateDecryptor(
    int cdm_id,
    mojo::PendingReceiver<media::mojom::Decryptor> receiver) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaInterfaceFactory::CreateDecryptor,
                                  weak_this_, cdm_id, std::move(receiver)));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateDecryptor(cdm_id, std::move(receiver));
}

#if BUILDFLAG(ENABLE_CDM_PROXY)
void MediaInterfaceFactory::CreateCdmProxy(
    const base::Token& cdm_guid,
    mojo::PendingReceiver<media::mojom::CdmProxy> receiver) {
  NOTREACHED() << "CdmProxy should only be connected from a library CDM";
}
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

media::mojom::InterfaceFactory*
MediaInterfaceFactory::GetMediaInterfaceFactory() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!media_interface_factory_) {
    remote_interfaces_->GetInterface(
        media_interface_factory_.BindNewPipeAndPassReceiver());
    media_interface_factory_.set_disconnect_handler(base::BindOnce(
        &MediaInterfaceFactory::OnConnectionError, base::Unretained(this)));
  }

  return media_interface_factory_.get();
}

void MediaInterfaceFactory::OnConnectionError() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  media_interface_factory_.reset();
}

}  // namespace content
