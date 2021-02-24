// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/interface_factory_impl.h"

#include <memory>
#include "base/bind.h"
#include "base/guid.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/services/mojo_decryptor_service.h"
#include "media/mojo/services/mojo_media_client.h"

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
#include "media/mojo/services/mojo_audio_decoder_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
#include "media/mojo/services/mojo_video_decoder_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER)
#include "base/callback_helpers.h"
#include "media/base/renderer.h"
#include "media/mojo/services/mojo_renderer_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "media/base/cdm_factory.h"
#include "media/mojo/services/mojo_cdm_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

#if defined(OS_WIN)
#include "media/mojo/services/media_foundation_renderer_wrapper.h"
#include "media/mojo/services/mojo_renderer_service.h"
#endif  // defined(OS_WIN)

namespace media {

InterfaceFactoryImpl::InterfaceFactoryImpl(
    mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces,
    MojoMediaClient* mojo_media_client)
    : frame_interfaces_(std::move(frame_interfaces)),
      mojo_media_client_(mojo_media_client) {
  DVLOG(1) << __func__;
  DCHECK(mojo_media_client_);

  SetReceiverDisconnectHandler();
}

InterfaceFactoryImpl::~InterfaceFactoryImpl() {
  DVLOG(1) << __func__;
}

// mojom::InterfaceFactory implementation.

void InterfaceFactoryImpl::CreateAudioDecoder(
    mojo::PendingReceiver<mojom::AudioDecoder> receiver) {
  DVLOG(2) << __func__;
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  scoped_refptr<base::SingleThreadTaskRunner> task_runner(
      base::ThreadTaskRunnerHandle::Get());

  std::unique_ptr<AudioDecoder> audio_decoder =
      mojo_media_client_->CreateAudioDecoder(task_runner);
  if (!audio_decoder) {
    DLOG(ERROR) << "AudioDecoder creation failed.";
    return;
  }

  audio_decoder_receivers_.Add(
      std::make_unique<MojoAudioDecoderService>(&cdm_service_context_,
                                                std::move(audio_decoder)),
      std::move(receiver));
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
}

void InterfaceFactoryImpl::CreateVideoDecoder(
    mojo::PendingReceiver<mojom::VideoDecoder> receiver) {
  DVLOG(2) << __func__;
#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  video_decoder_receivers_.Add(std::make_unique<MojoVideoDecoderService>(
                                   mojo_media_client_, &cdm_service_context_),
                               std::move(receiver));
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
}

void InterfaceFactoryImpl::CreateDefaultRenderer(
    const std::string& audio_device_id,
    mojo::PendingReceiver<mojom::Renderer> receiver) {
  DVLOG(2) << __func__;
#if BUILDFLAG(ENABLE_MOJO_RENDERER)
  auto renderer = mojo_media_client_->CreateRenderer(
      frame_interfaces_.get(), base::ThreadTaskRunnerHandle::Get(), &media_log_,
      audio_device_id);
  if (!renderer) {
    DLOG(ERROR) << "Renderer creation failed.";
    return;
  }

  std::unique_ptr<MojoRendererService> mojo_renderer_service =
      std::make_unique<MojoRendererService>(&cdm_service_context_,
                                            std::move(renderer));

  renderer_receivers_.Add(std::move(mojo_renderer_service),
                          std::move(receiver));
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void InterfaceFactoryImpl::CreateCastRenderer(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  DVLOG(2) << __func__;
  auto renderer = mojo_media_client_->CreateCastRenderer(
      frame_interfaces_.get(), base::ThreadTaskRunnerHandle::Get(), &media_log_,
      overlay_plane_id);
  if (!renderer) {
    DLOG(ERROR) << "Renderer creation failed.";
    return;
  }

  std::unique_ptr<MojoRendererService> mojo_renderer_service =
      std::make_unique<MojoRendererService>(&cdm_service_context_,
                                            std::move(renderer));

  renderer_receivers_.Add(std::move(mojo_renderer_service),
                          std::move(receiver));
}
#endif

#if defined(OS_ANDROID)
void InterfaceFactoryImpl::CreateMediaPlayerRenderer(
    mojo::PendingRemote<mojom::MediaPlayerRendererClientExtension>
        client_extension_ptr,
    mojo::PendingReceiver<mojom::Renderer> receiver,
    mojo::PendingReceiver<mojom::MediaPlayerRendererExtension>
        renderer_extension_receiver) {
  NOTREACHED();
}

void InterfaceFactoryImpl::CreateFlingingRenderer(
    const std::string& audio_device_id,
    mojo::PendingRemote<mojom::FlingingRendererClientExtension>
        client_extension,
    mojo::PendingReceiver<mojom::Renderer> receiver) {
  NOTREACHED();
}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
void InterfaceFactoryImpl::CreateMediaFoundationRenderer(
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver) {
  DVLOG(1) << __func__ << ": this=" << this;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  CreateMediaFoundationRendererOnTaskRunner(
      std::move(task_runner), std::move(receiver),
      std::move(renderer_extension_receiver));
}
#endif  // defined (OS_WIN)

void InterfaceFactoryImpl::CreateCdm(const std::string& key_system,
                                     const CdmConfig& cdm_config,
                                     CreateCdmCallback callback) {
  DVLOG(2) << __func__;
#if BUILDFLAG(ENABLE_MOJO_CDM)
  CdmFactory* cdm_factory = GetCdmFactory();
  if (!cdm_factory) {
    std::move(callback).Run(mojo::NullRemote(), base::nullopt,
                            mojo::NullRemote(), "CDM Factory creation failed");
    return;
  }

  MojoCdmService::Create(
      cdm_factory, &cdm_service_context_, key_system, cdm_config,
      base::BindOnce(&InterfaceFactoryImpl::OnCdmServiceCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
#else  // BUILDFLAG(ENABLE_MOJO_CDM)
  std::move(callback).Run(mojo::NullRemote(), base::nullopt, mojo::NullRemote(),
                          "Mojo CDM not supported");
#endif
}

void InterfaceFactoryImpl::OnDestroyPending(base::OnceClosure destroy_cb) {
  DVLOG(1) << __func__;
  destroy_cb_ = std::move(destroy_cb);
  if (IsEmpty())
    std::move(destroy_cb_).Run();
  // else the callback will be called when IsEmpty() becomes true.
}

bool InterfaceFactoryImpl::IsEmpty() {
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  if (!audio_decoder_receivers_.empty())
    return false;
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  if (!video_decoder_receivers_.empty())
    return false;
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
  if (!renderer_receivers_.empty())
    return false;
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)

#if BUILDFLAG(ENABLE_MOJO_CDM)
  if (!cdm_receivers_.empty())
    return false;
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

  if (!decryptor_receivers_.empty())
    return false;

  return true;
}

void InterfaceFactoryImpl::SetReceiverDisconnectHandler() {
  // base::Unretained is safe because all receivers are owned by |this|. If
  // |this| is destructed, the receivers will be destructed as well and the
  // disconnect handler should never be called.
  auto disconnect_cb = base::BindRepeating(
      &InterfaceFactoryImpl::OnReceiverDisconnect, base::Unretained(this));

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  audio_decoder_receivers_.set_disconnect_handler(disconnect_cb);
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  video_decoder_receivers_.set_disconnect_handler(disconnect_cb);
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
  renderer_receivers_.set_disconnect_handler(disconnect_cb);
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)

#if BUILDFLAG(ENABLE_MOJO_CDM)
  cdm_receivers_.set_disconnect_handler(disconnect_cb);
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

  decryptor_receivers_.set_disconnect_handler(disconnect_cb);
}

void InterfaceFactoryImpl::OnReceiverDisconnect() {
  DVLOG(2) << __func__;
  if (destroy_cb_ && IsEmpty())
    std::move(destroy_cb_).Run();
}

#if BUILDFLAG(ENABLE_MOJO_CDM)
CdmFactory* InterfaceFactoryImpl::GetCdmFactory() {
  if (!cdm_factory_) {
    cdm_factory_ =
        mojo_media_client_->CreateCdmFactory(frame_interfaces_.get());
    LOG_IF(ERROR, !cdm_factory_) << "CdmFactory not available.";
  }
  return cdm_factory_.get();
}

void InterfaceFactoryImpl::OnCdmServiceCreated(
    CreateCdmCallback callback,
    std::unique_ptr<MojoCdmService> cdm_service,
    mojo::PendingRemote<mojom::Decryptor> decryptor,
    const std::string& error_message) {
  if (!cdm_service) {
    std::move(callback).Run(mojo::NullRemote(), base::nullopt,
                            mojo::NullRemote(), error_message);
    return;
  }

  auto cdm_id = cdm_service->cdm_id();
  mojo::PendingRemote<mojom::ContentDecryptionModule> remote;
  cdm_receivers_.Add(std::move(cdm_service),
                     remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(remote), cdm_id, std::move(decryptor), "");
}

#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

#if defined(OS_WIN)
void InterfaceFactoryImpl::CreateMediaFoundationRendererOnTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver) {
  DVLOG(1) << __func__ << ": this=" << this;

  if (!task_runner->RunsTasksInCurrentSequence()) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InterfaceFactoryImpl::CreateMediaFoundationRendererOnTaskRunner,
            base::Unretained(this), task_runner, std::move(receiver),
            std::move(renderer_extension_receiver)));
    return;
  }

  DVLOG(1) << __func__ << ": this=" << this;

  auto renderer = std::make_unique<media::MediaFoundationRendererWrapper>(
      /*muted=*/false, std::move(task_runner),
      std::move(renderer_extension_receiver));

  media::MojoRendererService::Create(&cdm_service_context_, std::move(renderer),
                                     std::move(receiver));
}
#endif  // defined(OS_WIN)

}  // namespace media
