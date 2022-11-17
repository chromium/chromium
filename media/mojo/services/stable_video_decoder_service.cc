// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_service.h"

#include "media/mojo/common/media_type_converters.h"

namespace media {

StableVideoDecoderService::StableVideoDecoderService(
    std::unique_ptr<mojom::VideoDecoder> dst_video_decoder,
    MojoCdmServiceContext* cdm_service_context)
    : video_decoder_client_receiver_(this),
      media_log_receiver_(this),
      stable_video_frame_handle_releaser_receiver_(this),
      dst_video_decoder_(std::move(dst_video_decoder)),
      dst_video_decoder_receiver_(dst_video_decoder_.get())
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      cdm_service_context_(cdm_service_context)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
{
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!dst_video_decoder_);
  dst_video_decoder_remote_.Bind(
      dst_video_decoder_receiver_.BindNewPipeAndPassRemote());
}

StableVideoDecoderService::~StableVideoDecoderService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (cdm_id_)
    cdm_service_context_->UnregisterRemoteCdmContext(cdm_id_.value());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void StableVideoDecoderService::GetSupportedConfigs(
    GetSupportedConfigsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dst_video_decoder_remote_->GetSupportedConfigs(std::move(callback));
}

void StableVideoDecoderService::Construct(
    mojo::PendingAssociatedRemote<stable::mojom::VideoDecoderClient>
        stable_video_decoder_client_remote,
    mojo::PendingRemote<stable::mojom::MediaLog> stable_media_log_remote,
    mojo::PendingReceiver<stable::mojom::VideoFrameHandleReleaser>
        stable_video_frame_handle_releaser_receiver,
    mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
    const gfx::ColorSpace& target_color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (video_decoder_client_receiver_.is_bound()) {
    mojo::ReportBadMessage("Construct() already called");
    return;
  }

  DCHECK(!video_decoder_client_receiver_.is_bound());
  DCHECK(!stable_video_decoder_client_remote_.is_bound());
  stable_video_decoder_client_remote_.Bind(
      std::move(stable_video_decoder_client_remote));

  DCHECK(!media_log_receiver_.is_bound());
  DCHECK(!stable_media_log_remote_.is_bound());
  stable_media_log_remote_.Bind(std::move(stable_media_log_remote));

  DCHECK(!video_frame_handle_releaser_remote_.is_bound());
  DCHECK(!stable_video_frame_handle_releaser_receiver_.is_bound());
  stable_video_frame_handle_releaser_receiver_.Bind(
      std::move(stable_video_frame_handle_releaser_receiver));

  dst_video_decoder_remote_->Construct(
      video_decoder_client_receiver_.BindNewEndpointAndPassRemote(),
      media_log_receiver_.BindNewPipeAndPassRemote(),
      video_frame_handle_releaser_remote_.BindNewPipeAndPassReceiver(),
      std::move(decoder_buffer_pipe), mojom::CommandBufferIdPtr(),
      target_color_space);
}

void StableVideoDecoderService::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    mojo::PendingRemote<stable::mojom::StableCdmContext> cdm_context,
    InitializeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_decoder_client_receiver_.is_bound()) {
    DVLOG(2) << __func__ << " Construct() must be called first";
    std::move(callback).Run(DecoderStatus::Codes::kFailedToCreateDecoder,
                            /*needs_bitstream_conversion=*/false,
                            /*max_decode_requests=*/1,
                            VideoDecoderType::kUnknown);
    return;
  }

  // The |config| should have been validated at deserialization time.
  DCHECK(config.IsValidConfig());
  if (config.is_encrypted()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (!cdm_id_) {
      if (!cdm_context) {
        std::move(callback).Run(DecoderStatus::Codes::kMissingCDM,
                                /*needs_bitstream_conversion=*/false,
                                /*max_decode_requests=*/1,
                                VideoDecoderType::kUnknown);
        return;
      }
      remote_cdm_context_ = base::WrapRefCounted(
          new chromeos::RemoteCdmContext(std::move(cdm_context)));
      cdm_id_ = cdm_service_context_->RegisterRemoteCdmContext(
          remote_cdm_context_.get());
    }
#else
    std::move(callback).Run(DecoderStatus::Codes::kUnsupportedConfig,
                            /*needs_bitstream_conversion=*/false,
                            /*max_decode_requests=*/1,
                            VideoDecoderType::kUnknown);
    return;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  // Even though this is in-process, we still need to pass a |cdm_id_|
  // instead of a media::CdmContext* since this goes through Mojo IPC. This is
  // why we need to register with the |cdm_service_context_| above.
  dst_video_decoder_remote_->Initialize(config, low_delay, cdm_id_,
                                        std::move(callback));
}

void StableVideoDecoderService::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    DecodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_decoder_client_receiver_.is_bound()) {
    DVLOG(2) << __func__ << " Construct() must be called first";
    std::move(callback).Run(DecoderStatus::Codes::kFailedToCreateDecoder);
    return;
  }

  CHECK(buffer);
  mojom::DecoderBufferPtr mojo_buffer = mojom::DecoderBuffer::From(*buffer);
  CHECK(mojo_buffer);
  dst_video_decoder_remote_->Decode(std::move(mojo_buffer),
                                    std::move(callback));
}

void StableVideoDecoderService::Reset(ResetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!video_decoder_client_receiver_.is_bound()) {
    DVLOG(2) << __func__ << " Construct() must be called first";
    std::move(callback).Run();
    return;
  }
  dst_video_decoder_remote_->Reset(std::move(callback));
}

void StableVideoDecoderService::ReleaseVideoFrame(
    const base::UnguessableToken& release_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video_frame_handle_releaser_remote_.is_bound());
  // Note: we don't pass a gpu::SyncToken because it's assumed that the client
  // (the GPU process) has already waited on the SyncToken that comes from the
  // ultimate client (the renderer process) before calling ReleaseVideoFrame()
  // on the out-of-process video decoder.
  video_frame_handle_releaser_remote_->ReleaseVideoFrame(
      release_token, /*release_sync_token=*/absl::nullopt);
}

void StableVideoDecoderService::OnVideoFrameDecoded(
    const scoped_refptr<VideoFrame>& frame,
    bool can_read_without_stalling,
    const absl::optional<base::UnguessableToken>& release_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stable_video_decoder_client_remote_.is_bound());
  DCHECK(release_token.has_value());
  stable_video_decoder_client_remote_->OnVideoFrameDecoded(
      frame, can_read_without_stalling, *release_token);
}

void StableVideoDecoderService::OnWaiting(WaitingReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stable_video_decoder_client_remote_.is_bound());
  stable_video_decoder_client_remote_->OnWaiting(reason);
}

void StableVideoDecoderService::RequestOverlayInfo(
    bool restart_for_transitions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void StableVideoDecoderService::AddLogRecord(const MediaLogRecord& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stable_media_log_remote_.is_bound());
  stable_media_log_remote_->AddLogRecord(event);
}

}  // namespace media
