// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_interface_factory.h"

#include <string>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace content {

MediaInterfaceFactory::MediaInterfaceFactory(
    const blink::BrowserInterfaceBrokerProxy* interface_broker)
    : interface_broker_(interface_broker) {
  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  weak_this_ = weak_factory_.GetWeakPtr();
}

MediaInterfaceFactory::MediaInterfaceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingRemote<media::mojom::InterfaceFactory> interface_factory)
    : media_interface_factory_(std::move(interface_factory)),
      task_runner_(std::move(task_runner)) {
  // `interface_broker_` remains null, but we don't need it since we already
  // have `media_interface_factory_`.
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
    mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
    mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
        dst_video_decoder) {
  // The renderer process cannot act as a proxy for video decoding.
  DCHECK(!dst_video_decoder);
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MediaInterfaceFactory::CreateVideoDecoder, weak_this_,
            std::move(receiver),
            /*dst_video_decoder=*/
            mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>()));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateVideoDecoder(std::move(receiver),
                                                 /*dst_video_decoder=*/{});
}

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
void MediaInterfaceFactory::CreateStableVideoDecoder(
    mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
        video_decoder) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaInterfaceFactory::CreateStableVideoDecoder,
                       weak_this_, std::move(video_decoder)));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateStableVideoDecoder(
      std::move(video_decoder));
}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

void MediaInterfaceFactory::CreateAudioEncoder(
    mojo::PendingReceiver<media::mojom::AudioEncoder> receiver) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaInterfaceFactory::CreateAudioEncoder,
                                  weak_this_, std::move(receiver)));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateAudioEncoder(std::move(receiver));
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

#if BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
void MediaInterfaceFactory::CreateMediaFoundationRenderer(
    mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver,
    mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
        client_extension_remote) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaInterfaceFactory::CreateMediaFoundationRenderer,
                       weak_this_, std::move(media_log_remote),
                       std::move(receiver),
                       std::move(renderer_extension_receiver),
                       std::move(client_extension_remote)));
    return;
  }

  DVLOG(1) << __func__;
  GetMediaInterfaceFactory()->CreateMediaFoundationRenderer(
      std::move(media_log_remote), std::move(receiver),
      std::move(renderer_extension_receiver),
      std::move(client_extension_remote));
}
#endif  // BUILDFLAG(IS_WIN)

void MediaInterfaceFactory::CreateCdm(const media::CdmConfig& cdm_config,
                                      CreateCdmCallback callback) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaInterfaceFactory::CreateCdm, weak_this_,
                                  cdm_config, std::move(callback)));
    return;
  }

  DVLOG(1) << __func__ << ": cdm_config=" << cdm_config;
  GetMediaInterfaceFactory()->CreateCdm(cdm_config, std::move(callback));
}

media::mojom::InterfaceFactory*
MediaInterfaceFactory::GetMediaInterfaceFactory() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!media_interface_factory_) {
    interface_broker_->GetInterface(
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
