// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/interface_factory_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/services/mojo_decryptor_service.h"
#include "media/mojo/services/mojo_media_client.h"

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "media/mojo/services/mojo_audio_decoder_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_AUDIO_ENCODER)
#include "media/mojo/services/mojo_audio_encoder_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_ENCODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
#include "media/mojo/services/mojo_video_decoder_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER) || \
    BUILDFLAG(IS_WIN)
#include "base/functional/callback_helpers.h"
#include "media/base/renderer.h"
#include "media/mojo/services/mojo_renderer_service.h"
#endif

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "media/base/cdm_factory.h"
#include "media/mojo/services/mojo_cdm_service.h"
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

namespace media {

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
// The class creates MojoAudioDecoderService on caller's thread and runs the
// decoder on a high priority background thread. This can help avoid audio
// decoder underflow in audio renderer. In addition, it also improves video
// decoder performance by moving busy audio tasks off video decoder thread.
class InterfaceFactoryImpl::AudioDecoderReceivers {
 public:
  AudioDecoderReceivers(MojoMediaClient* mojo_media_client,
                        MojoCdmServiceContext* mojo_cdm_service_context,
                        base::RepeatingClosure disconnect_handler)
      : task_runner_(
            base::FeatureList::IsEnabled(
                kUseTaskRunnerForMojoAudioDecoderService)
                ? base::ThreadPool::CreateSingleThreadTaskRunner(
                      {base::TaskPriority::USER_BLOCKING, base::MayBlock()})
                : base::SingleThreadTaskRunner::GetCurrentDefault()),
        receivers_(task_runner_),
        mojo_media_client_(mojo_media_client),
        mojo_cdm_service_context_(mojo_cdm_service_context),
        disconnect_handler_(disconnect_handler) {
    DCHECK(mojo_media_client_);
    DCHECK(mojo_cdm_service_context_);

    base::RepeatingClosure disconnect_cb =
        base::BindRepeating(&AudioDecoderReceivers::OnReceiverDisconnect,
                            weak_factory_.GetWeakPtr());
    if (!task_runner_->RunsTasksInCurrentSequence()) {
      disconnect_cb =
          base::BindPostTaskToCurrentDefault(std::move(disconnect_cb));
    }

    receivers_
        .AsyncCall(&mojo::UniqueReceiverSet<
                   mojom::AudioDecoder>::set_disconnect_handler)
        .WithArgs(std::move(disconnect_cb));
  }

  ~AudioDecoderReceivers() = default;

  void CreateAudioDecoder(mojo::PendingReceiver<mojom::AudioDecoder> receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    typedef mojo::ReceiverId (
        mojo::UniqueReceiverSet<mojom::AudioDecoder>::*AddFuncType)(
        std::unique_ptr<mojom::AudioDecoder>,
        mojo::PendingReceiver<mojom::AudioDecoder>,
        scoped_refptr<base::SequencedTaskRunner>);

    receivers_
        .AsyncCall(base::IgnoreResult<AddFuncType>(
            &mojo::UniqueReceiverSet<mojom::AudioDecoder>::Add))
        .WithArgs(
            std::make_unique<MojoAudioDecoderService>(
                mojo_media_client_, mojo_cdm_service_context_, task_runner_),
            std::move(receiver), task_runner_);
    ++receiver_count_;
  }

  bool IsEmpty() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return receiver_count_ == 0;
  }

 private:
  void OnReceiverDisconnect() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    --receiver_count_;
    disconnect_handler_.Run();
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::SequenceBound<mojo::UniqueReceiverSet<mojom::AudioDecoder>> receivers_;

  // The following variables run on the caller thread.
  const raw_ptr<MojoMediaClient> mojo_media_client_;
  const raw_ptr<MojoCdmServiceContext> mojo_cdm_service_context_;
  base::RepeatingClosure disconnect_handler_;
  int receiver_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AudioDecoderReceivers> weak_factory_{this};
};
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

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
  if (!audio_decoder_receivers_) {
    audio_decoder_receivers_ = std::make_unique<AudioDecoderReceivers>(
        mojo_media_client_, &cdm_service_context_,
        // Unretained is safe here because InterfaceFactoryImpl is
        // DeferredDestroy and it will wait for all the mojo channel
        // disconnection before destructing itself.
        base::BindRepeating(&InterfaceFactoryImpl::OnReceiverDisconnect,
                            base::Unretained(this)));
  }

  audio_decoder_receivers_->CreateAudioDecoder(std::move(receiver));
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
}

void InterfaceFactoryImpl::CreateVideoDecoder(
    mojo::PendingReceiver<mojom::VideoDecoder> receiver,
    mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
        dst_video_decoder) {
  DVLOG(2) << __func__;
#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  // When out-of-process video decoding is enabled, we need to ensure that we
  // know the supported video decoder configurations prior to creating the
  // MojoVideoDecoderService. That way, the MojoVideoDecoderService won't need
  // to talk to the out-of-process video decoder to find out the supported
  // configurations (this would be a problem because the MojoVideoDecoderService
  // may not have an easy way to talk to the out-of-process decoder at the time
  // the supported configurations are needed).
  mojo_media_client_->NotifyDecoderSupportKnown(
      std::move(dst_video_decoder),
      base::BindOnce(&InterfaceFactoryImpl::FinishCreatingVideoDecoder,
                     weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
#else
  video_decoder_receivers_.Add(std::make_unique<MojoVideoDecoderService>(
                                   mojo_media_client_, &cdm_service_context_,
                                   std::move(dst_video_decoder)),
                               std::move(receiver));
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
}

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
void InterfaceFactoryImpl::CreateStableVideoDecoder(
    mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
        video_decoder) {
  // The browser process ensures that this is not called in the GPU process.
  NOTREACHED();
}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

void InterfaceFactoryImpl::CreateAudioEncoder(
    mojo::PendingReceiver<mojom::AudioEncoder> receiver) {
#if BUILDFLAG(ENABLE_MOJO_AUDIO_ENCODER)
  auto runner = base::SingleThreadTaskRunner::GetCurrentDefault();

  auto underlying_encoder = mojo_media_client_->CreateAudioEncoder(runner);
  if (!underlying_encoder) {
    DLOG(ERROR) << "AudioEncoder creation failed.";
    return;
  }

  audio_encoder_receivers_.Add(
      std::make_unique<MojoAudioEncoderService>(std::move(underlying_encoder)),
      std::move(receiver));
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_ENCODER)
}

void InterfaceFactoryImpl::CreateDefaultRenderer(
    const std::string& audio_device_id,
    mojo::PendingReceiver<mojom::Renderer> receiver) {
  DVLOG(2) << __func__;
#if BUILDFLAG(ENABLE_MOJO_RENDERER)
  auto renderer = mojo_media_client_->CreateRenderer(
      frame_interfaces_.get(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), &media_log_,
      audio_device_id);
  if (!renderer) {
    DLOG(ERROR) << "Renderer creation failed.";
    return;
  }

  AddRenderer(std::move(renderer), std::move(receiver));
#endif  // BUILDFLAG(ENABLE_MOJO_RENDERER)
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
void InterfaceFactoryImpl::CreateCastRenderer(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingReceiver<media::mojom::Renderer> receiver) {
  DVLOG(2) << __func__;
  auto renderer = mojo_media_client_->CreateCastRenderer(
      frame_interfaces_.get(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), &media_log_,
      overlay_plane_id);
  if (!renderer) {
    DLOG(ERROR) << "Renderer creation failed.";
    return;
  }

  AddRenderer(std::move(renderer), std::move(receiver));
}
#endif

#if BUILDFLAG(IS_ANDROID)
void InterfaceFactoryImpl::CreateMediaPlayerRenderer(
    mojo::PendingRemote<mojom::MediaPlayerRendererClientExtension>
        client_extension_ptr,
    mojo::PendingReceiver<mojom::Renderer> receiver,
    mojo::PendingReceiver<mojom::MediaPlayerRendererExtension>
        renderer_extension_receiver) {
  NOTREACHED_IN_MIGRATION();
}

void InterfaceFactoryImpl::CreateFlingingRenderer(
    const std::string& audio_device_id,
    mojo::PendingRemote<mojom::FlingingRendererClientExtension>
        client_extension,
    mojo::PendingReceiver<mojom::Renderer> receiver) {
  NOTREACHED_IN_MIGRATION();
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
void InterfaceFactoryImpl::CreateMediaFoundationRenderer(
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<media::mojom::Renderer> receiver,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver,
    mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
        client_extension_remote) {
  DVLOG(2) << __func__;
  auto renderer = mojo_media_client_->CreateMediaFoundationRenderer(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      frame_interfaces_.get(), std::move(media_log_remote),
      std::move(renderer_extension_receiver),
      std::move(client_extension_remote));
  if (!renderer) {
    DLOG(ERROR) << "MediaFoundationRenderer creation failed.";
    return;
  }

  AddRenderer(std::move(renderer), std::move(receiver));
}
#endif  // BUILDFLAG(IS_WIN)

void InterfaceFactoryImpl::CreateCdm(const CdmConfig& cdm_config,
                                     CreateCdmCallback callback) {
  DVLOG(2) << __func__;
#if BUILDFLAG(ENABLE_MOJO_CDM)
  CdmFactory* cdm_factory = GetCdmFactory();
  if (!cdm_factory) {
    std::move(callback).Run(mojo::NullRemote(), nullptr,
                            CreateCdmStatus::kCdmFactoryCreationFailed);
    return;
  }

  auto mojo_cdm_service =
      std::make_unique<MojoCdmService>(&cdm_service_context_);
  auto* raw_mojo_cdm_service = mojo_cdm_service.get();
  DCHECK(!pending_mojo_cdm_services_.count(raw_mojo_cdm_service));
  pending_mojo_cdm_services_[raw_mojo_cdm_service] =
      std::move(mojo_cdm_service);
  raw_mojo_cdm_service->Initialize(
      cdm_factory, cdm_config,
      base::BindOnce(&InterfaceFactoryImpl::OnCdmServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr(), raw_mojo_cdm_service,
                     std::move(callback)));
#else  // BUILDFLAG(ENABLE_MOJO_CDM)
  std::move(callback).Run(mojo::NullRemote(), nullptr,
                          CreateCdmStatus::kCdmNotSupported);
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
  if (audio_decoder_receivers_ && !audio_decoder_receivers_->IsEmpty()) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  if (!video_decoder_receivers_.empty())
    return false;
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_AUDIO_ENCODER)
  if (!audio_encoder_receivers_.empty())
    return false;
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_ENCODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER) || \
    BUILDFLAG(IS_WIN)
  if (!renderer_receivers_.empty())
    return false;
#endif

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

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  video_decoder_receivers_.set_disconnect_handler(disconnect_cb);
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ENABLE_MOJO_AUDIO_ENCODER)
  audio_encoder_receivers_.set_disconnect_handler(disconnect_cb);
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_ENCODER)

#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER) || \
    BUILDFLAG(IS_WIN)
  renderer_receivers_.set_disconnect_handler(disconnect_cb);
#endif

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

#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(ENABLE_CAST_RENDERER) || \
    BUILDFLAG(IS_WIN)
void InterfaceFactoryImpl::AddRenderer(
    std::unique_ptr<media::Renderer> renderer,
    mojo::PendingReceiver<mojom::Renderer> receiver) {
  auto mojo_renderer_service = std::make_unique<MojoRendererService>(
      &cdm_service_context_, std::move(renderer));
  renderer_receivers_.Add(std::move(mojo_renderer_service),
                          std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_MOJO_CDM)
CdmFactory* InterfaceFactoryImpl::GetCdmFactory() {
  if (!cdm_factory_) {
    cdm_factory_ =
        mojo_media_client_->CreateCdmFactory(frame_interfaces_.get());
    LOG_IF(ERROR, !cdm_factory_) << "CdmFactory not available.";
  }
  return cdm_factory_.get();
}

void InterfaceFactoryImpl::OnCdmServiceInitialized(
    MojoCdmService* raw_mojo_cdm_service,
    CreateCdmCallback callback,
    mojom::CdmContextPtr cdm_context,
    CreateCdmStatus status) {
  DCHECK(raw_mojo_cdm_service);

  // Remove pending MojoCdmService from the mapping in all cases.
  DCHECK(pending_mojo_cdm_services_.count(raw_mojo_cdm_service));
  auto mojo_cdm_service =
      std::move(pending_mojo_cdm_services_[raw_mojo_cdm_service]);
  pending_mojo_cdm_services_.erase(raw_mojo_cdm_service);

  if (!cdm_context) {
    std::move(callback).Run(mojo::NullRemote(), nullptr, status);
    return;
  }

  mojo::PendingRemote<mojom::ContentDecryptionModule> remote;
  cdm_receivers_.Add(std::move(mojo_cdm_service),
                     remote.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(std::move(remote), std::move(cdm_context),
                          CreateCdmStatus::kSuccess);
}

#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
void InterfaceFactoryImpl::FinishCreatingVideoDecoder(
    mojo::PendingReceiver<mojom::VideoDecoder> receiver,
    mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
        dst_video_decoder) {
#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
  video_decoder_receivers_.Add(std::make_unique<MojoVideoDecoderService>(
                                   mojo_media_client_, &cdm_service_context_,
                                   std::move(dst_video_decoder)),
                               std::move(receiver));
#else
  NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

}  // namespace media
