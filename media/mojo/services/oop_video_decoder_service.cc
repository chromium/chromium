// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/oop_video_decoder_service.h"

#include "base/notreached.h"
#include "media/gpu/chromeos/frame_registry.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/validation_utils.h"

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace media {

OOPVideoDecoderService::OOPVideoDecoderService(
    mojo::PendingRemote<mojom::VideoDecoderTracker> tracker_remote,
    std::unique_ptr<mojom::VideoDecoder> dst_video_decoder,
    MojoCdmServiceContext* cdm_service_context)
    : tracker_remote_(std::move(tracker_remote)),
      video_decoder_client_receiver_(this),
      media_log_receiver_(this),
      video_frame_handle_releaser_receiver_(this),
      dst_video_decoder_(std::move(dst_video_decoder)),
      dst_video_decoder_receiver_(dst_video_decoder_.get())
#if BUILDFLAG(IS_CHROMEOS)
      ,
      cdm_service_context_(cdm_service_context)
#endif  // BUILDFLAG(IS_CHROMEOS)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!dst_video_decoder_);
  dst_video_decoder_remote_.Bind(
      dst_video_decoder_receiver_.BindNewPipeAndPassRemote());
}

OOPVideoDecoderService::~OOPVideoDecoderService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_CHROMEOS)
  if (cdm_id_) {
    cdm_service_context_->UnregisterRemoteCdmContext(cdm_id_.value());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void OOPVideoDecoderService::GetSupportedConfigs(
    GetSupportedConfigsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dst_video_decoder_remote_->GetSupportedConfigs(std::move(callback));
}

void OOPVideoDecoderService::Construct(
    mojo::PendingAssociatedRemote<mojom::VideoDecoderClient>
        video_decoder_client_remote,
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<mojom::VideoFrameHandleReleaser>
        video_frame_handle_releaser_receiver,
    mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
    mojom::CommandBufferIdPtr command_buffer_id,
    const gfx::ColorSpace& target_color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_decoder_client_receiver_.is_bound()) {
    mojo::ReportBadMessage("Construct() already called");
    return;
  }

  DCHECK(!video_decoder_client_receiver_.is_bound());
  DCHECK(!video_decoder_client_remote_.is_bound());
  video_decoder_client_remote_.Bind(std::move(video_decoder_client_remote));

  DCHECK(!media_log_receiver_.is_bound());
  DCHECK(!media_log_remote_.is_bound());
  media_log_remote_.Bind(std::move(media_log_remote));

  DCHECK(!video_frame_handle_releaser_remote_.is_bound());
  DCHECK(!video_frame_handle_releaser_receiver_.is_bound());
  video_frame_handle_releaser_receiver_.Bind(
      std::move(video_frame_handle_releaser_receiver));

  dst_video_decoder_remote_->Construct(
      video_decoder_client_receiver_.BindNewEndpointAndPassRemote(),
      media_log_receiver_.BindNewPipeAndPassRemote(),
      video_frame_handle_releaser_remote_.BindNewPipeAndPassReceiver(),
      std::move(decoder_buffer_pipe), mojom::CommandBufferIdPtr(),
      target_color_space);
}

void OOPVideoDecoderService::Initialize(const VideoDecoderConfig& config,
                                        bool low_delay,
                                        mojom::CdmPtr cdm,
                                        InitializeCallback callback) {
  // The client of the OOPVideoDecoderService is the OOPVideoDecoder which lives
  // in the GPU process and is therefore up the trust gradient. The
  // OOPVideoDecoder doesn't call Initialize() with a cdm id (it calls
  // Initialize() with a cdm context instead). Thus, it's appropriate to crash
  // here via a NOTREACHED().
  if (cdm && !cdm->is_cdm_context()) {
    NOTREACHED();
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_decoder_client_receiver_.is_bound()) {
    DVLOG(2) << __func__ << " Construct() must be called first";
    std::move(callback).Run(DecoderStatus::Codes::kFailedToCreateDecoder,
                            /*needs_bitstream_conversion=*/false,
                            /*max_decode_requests=*/1,
                            VideoDecoderType::kUnknown,
                            /*needs_transcryption=*/false);
    return;
  }

  // The |config| should have been validated at deserialization time.
  DCHECK(config.IsValidConfig());
  if (config.is_encrypted()) {
#if BUILDFLAG(IS_CHROMEOS)
    if (!cdm_id_) {
      if (!cdm) {
        std::move(callback).Run(DecoderStatus::Codes::kMissingCDM,
                                /*needs_bitstream_conversion=*/false,
                                /*max_decode_requests=*/1,
                                VideoDecoderType::kUnknown,
                                /*needs_transcryption=*/false);
        return;
      }
      // We have already validated above that if a |cdm| is  provided, it must
      // be a cdm context.
      CHECK(cdm->is_cdm_context());
      mojo::PendingRemote<mojom::CdmContextForOOPVD> cdm_context =
          std::move(cdm->get_cdm_context());
      // The cdm context is non-nullable inside the |cdm| union.
      CHECK(!!cdm_context);
      remote_cdm_context_ = base::WrapRefCounted(
          new chromeos::RemoteCdmContext(std::move(cdm_context)));
      cdm_id_ = cdm_service_context_->RegisterRemoteCdmContext(
          remote_cdm_context_.get());
    }
#else
    std::move(callback).Run(DecoderStatus::Codes::kUnsupportedConfig,
                            /*needs_bitstream_conversion=*/false,
                            /*max_decode_requests=*/1,
                            VideoDecoderType::kUnknown,
                            /*needs_transcryption=*/false);
    return;
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  // Even though this is in-process, we still need to pass a |cdm_id_|
  // instead of a media::CdmContext* since this goes through Mojo IPC. This is
  // why we need to register with the |cdm_service_context_| above.
  //
  // Note: base::Unretained() is safe because *|this| fully owns
  // |dst_video_decoder_remote_|, so the response callback will never run beyond
  // the lifetime of *|this|.
  dst_video_decoder_remote_->Initialize(
      config, low_delay,
      cdm_id_ ? mojom::Cdm::NewCdmId(cdm_id_.value()) : nullptr,
      base::BindOnce(&OOPVideoDecoderService::OnInitializeDone,
                     base::Unretained(this), std::move(callback)));
}

void OOPVideoDecoderService::OnInitializeDone(InitializeCallback init_cb,
                                              const DecoderStatus& status,
                                              bool needs_bitstream_conversion,
                                              int32_t max_decode_requests,
                                              VideoDecoderType decoder_type,
                                              bool needs_transcryption) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  needs_transcryption = false;
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_VAAPI)
  if (cdm_id_) {
    needs_transcryption = (VaapiWrapper::GetImplementationType() ==
                           VAImplementation::kMesaGallium);
  }
#endif
  std::move(init_cb).Run(status, needs_bitstream_conversion,
                         max_decode_requests, decoder_type,
                         needs_transcryption);
}

void OOPVideoDecoderService::Decode(mojom::DecoderBufferPtr buffer,
                                    DecodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_decoder_client_receiver_.is_bound()) {
    DVLOG(2) << __func__ << " Construct() must be called first";
    std::move(callback).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  // TODO(crbug.com/390706725): remove this check once the extra validation in
  // ValidateAndConvertMojoDecoderBuffer() is merged into
  // media_type_converters.cc.
  scoped_refptr<media::DecoderBuffer> media_decoder_buffer =
      ValidateAndConvertMojoDecoderBuffer(std::move(buffer));
  CHECK(media_decoder_buffer);
  mojom::DecoderBufferPtr mojo_buffer =
      mojom::DecoderBuffer::From(*media_decoder_buffer);
  CHECK(mojo_buffer);
  dst_video_decoder_remote_->Decode(std::move(mojo_buffer),
                                    std::move(callback));
}

void OOPVideoDecoderService::Reset(ResetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_decoder_client_receiver_.is_bound()) {
    DVLOG(2) << __func__ << " Construct() must be called first";
    std::move(callback).Run();
    return;
  }
  dst_video_decoder_remote_->Reset(std::move(callback));
}

void OOPVideoDecoderService::OnOverlayInfoChanged(
    const OverlayInfo& overlay_info) {
  // The client of the OOPVideoDecoderService is the OOPVideoDecoder which lives
  // in the GPU process and is therefore up the trust gradient. The
  // OOPVideoDecoder doesn't call OnOverlayInfoChanged(). Thus, it's appropriate
  // to crash here via a NOTREACHED().
  NOTREACHED();
}

void OOPVideoDecoderService::ReleaseVideoFrame(
    const base::UnguessableToken& release_token,
    const std::optional<gpu::SyncToken>& release_sync_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_frame_handle_releaser_remote_.is_bound());
  // Note: we don't pass a gpu::SyncToken because it's assumed that the client
  // (the GPU process) has already waited on the SyncToken that comes from the
  // ultimate client (the renderer process) before calling ReleaseVideoFrame()
  // on the out-of-process video decoder.
  video_frame_handle_releaser_remote_->ReleaseVideoFrame(
      release_token, /*release_sync_token=*/std::nullopt);
}

void OOPVideoDecoderService::OnVideoFrameDecoded(
    const scoped_refptr<VideoFrame>& frame,
    bool can_read_without_stalling,
    const std::optional<base::UnguessableToken>& release_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_decoder_client_remote_.is_bound());
  DCHECK(release_token.has_value());

  // The mojo traits have been coded assuming these conditions.
  CHECK(frame->metadata().allow_overlay);
  CHECK(!frame->metadata().end_of_stream);
  CHECK(frame->metadata().power_efficient);
  CHECK(!frame->HasMappableGpuBuffer());

  video_decoder_client_remote_->OnVideoFrameDecoded(
      std::move(frame), can_read_without_stalling, *release_token);
}

void OOPVideoDecoderService::OnWaiting(WaitingReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_decoder_client_remote_.is_bound());
  video_decoder_client_remote_->OnWaiting(reason);
}

void OOPVideoDecoderService::RequestOverlayInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void OOPVideoDecoderService::AddLogRecord(const MediaLogRecord& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_log_remote_.is_bound());
  media_log_remote_->AddLogRecord(event);
}

}  // namespace media
