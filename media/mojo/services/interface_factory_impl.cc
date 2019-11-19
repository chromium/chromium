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
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
#include "media/mojo/services/mojo_audio_decoder_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
#include "media/mojo/services/mojo_video_decoder_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER)
#include "base/bind_helpers.h"
#include "media/base/renderer.h"
#include "media/mojo/services/mojo_renderer_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "media/base/cdm_factory.h"
#include "media/mojo/services/mojo_cdm_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

#if BUILDFLAG(ENABLE_CDM_PROXY)
#include "media/mojo/services/mojo_cdm_proxy_service.h"
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

namespace media {

InterfaceFactoryImpl::InterfaceFactoryImpl(
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        host_interfaces,
    std::unique_ptr<service_manager::ServiceKeepaliveRef> keepalive_ref,
    MojoMediaClient* mojo_media_client)
    : host_interfaces_(std::move(host_interfaces)),
      keepalive_ref_(std::move(keepalive_ref)),
      mojo_media_client_(mojo_media_client) {
  DVLOG(1) << __func__;
  DCHECK(mojo_media_client_);

  SetBindingConnectionErrorHandler();
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
      host_interfaces_.get(), base::ThreadTaskRunnerHandle::Get(), &media_log_,
      audio_device_id);
  if (!renderer) {
    DLOG(ERROR) << "Renderer creation failed.";
    return;
  }

  std::unique_ptr<MojoRendererService> mojo_renderer_service =
      std::make_unique<MojoRendererService>(&cdm_service_context_,
                                            std::move(renderer));

  MojoRendererService* mojo_renderer_service_ptr = mojo_renderer_service.get();

  mojo::ReceiverId receiver_id = renderer_receivers_.Add(
      std::move(mojo_renderer_service), std::move(receiver));

  // base::Unretained() is safe because the callback will be fired by
  // |mojo_renderer_service|, which is owned by |renderer_receivers_|.
  mojo_renderer_service_ptr->set_bad_message_cb(base::Bind(
      base::IgnoreResult(&mojo::UniqueReceiverSet<mojom::Renderer>::Remove),
      base::Unretained(&renderer_receivers_), receiver_id));
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void InterfaceFactoryImpl::CreateCastRenderer(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  DVLOG(2) << __func__;
  auto renderer = mojo_media_client_->CreateCastRenderer(
      host_interfaces_.get(), base::ThreadTaskRunnerHandle::Get(), &media_log_,
      overlay_plane_id);
  if (!renderer) {
    DLOG(ERROR) << "Renderer creation failed.";
    return;
  }

  std::unique_ptr<MojoRendererService> mojo_renderer_service =
      std::make_unique<MojoRendererService>(&cdm_service_context_,
                                            std::move(renderer));

  MojoRendererService* mojo_renderer_service_ptr = mojo_renderer_service.get();

  mojo::ReceiverId receiver_id = renderer_receivers_.Add(
      std::move(mojo_renderer_service), std::move(receiver));

  // base::Unretained() is safe because the callback will be fired by
  // |mojo_renderer_service|, which is owned by |renderer_receivers_|.
  mojo_renderer_service_ptr->set_bad_message_cb(base::BindRepeating(
      base::IgnoreResult(&mojo::UniqueReceiverSet<mojom::Renderer>::Remove),
      base::Unretained(&renderer_receivers_), receiver_id));
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

void InterfaceFactoryImpl::CreateCdm(
    const std::string& /* key_system */,
    mojo::PendingReceiver<mojom::ContentDecryptionModule> receiver) {
  DVLOG(2) << __func__;
#if BUILDFLAG(ENABLE_MOJO_CDM)
  CdmFactory* cdm_factory = GetCdmFactory();
  if (!cdm_factory)
    return;

  cdm_receivers_.Add(
      std::make_unique<MojoCdmService>(cdm_factory, &cdm_service_context_),
      std::move(receiver));
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)
}

void InterfaceFactoryImpl::CreateDecryptor(
    int cdm_id,
    mojo::PendingReceiver<mojom::Decryptor> receiver) {
  DVLOG(2) << __func__;
  auto mojo_decryptor_service =
      MojoDecryptorService::Create(cdm_id, &cdm_service_context_);
  if (!mojo_decryptor_service) {
    DLOG(ERROR) << "MojoDecryptorService creation failed.";
    return;
  }

  decryptor_receivers_.Add(std::move(mojo_decryptor_service),
                           std::move(receiver));
}
#if BUILDFLAG(ENABLE_CDM_PROXY)
void InterfaceFactoryImpl::CreateCdmProxy(
    const base::Token& cdm_guid,
    mojo::PendingReceiver<mojom::CdmProxy> receiver) {
  DVLOG(2) << __func__;
  auto cdm_proxy = mojo_media_client_->CreateCdmProxy(cdm_guid);
  if (!cdm_proxy) {
    DLOG(ERROR) << "CdmProxy creation failed.";
    return;
  }

  cdm_proxy_receivers_.Add(std::make_unique<MojoCdmProxyService>(
                               std::move(cdm_proxy), &cdm_service_context_),
                           std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

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

#if BUILDFLAG(ENABLE_CDM_PROXY)
  if (!cdm_proxy_receivers_.empty())
    return false;
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

  if (!decryptor_receivers_.empty())
    return false;

  return true;
}

void InterfaceFactoryImpl::SetBindingConnectionErrorHandler() {
  // base::Unretained is safe because all receivers are owned by |this|. If
  // |this| is destructed, the receivers will be destructed as well and the
  // connection error handler should never be called.
  auto connection_error_cb = base::BindRepeating(
      &InterfaceFactoryImpl::OnBindingConnectionError, base::Unretained(this));

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  audio_decoder_receivers_.set_disconnect_handler(connection_error_cb);
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  video_decoder_receivers_.set_disconnect_handler(connection_error_cb);
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER)
  renderer_receivers_.set_disconnect_handler(connection_error_cb);
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)

#if BUILDFLAG(ENABLE_MOJO_CDM)
  cdm_receivers_.set_disconnect_handler(connection_error_cb);
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

#if BUILDFLAG(ENABLE_CDM_PROXY)
  cdm_proxy_receivers_.set_disconnect_handler(connection_error_cb);
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

  decryptor_receivers_.set_disconnect_handler(connection_error_cb);
}

void InterfaceFactoryImpl::OnBindingConnectionError() {
  DVLOG(2) << __func__;
  if (destroy_cb_ && IsEmpty())
    std::move(destroy_cb_).Run();
}

#if BUILDFLAG(ENABLE_MOJO_CDM)
CdmFactory* InterfaceFactoryImpl::GetCdmFactory() {
  if (!cdm_factory_) {
    cdm_factory_ = mojo_media_client_->CreateCdmFactory(host_interfaces_.get());
    LOG_IF(ERROR, !cdm_factory_) << "CdmFactory not available.";
  }
  return cdm_factory_.get();
}
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

}  // namespace media
