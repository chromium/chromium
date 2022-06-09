// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_service.h"

namespace media {

StableVideoDecoderService::StableVideoDecoderService(
    std::unique_ptr<mojom::VideoDecoder> dst_video_decoder)
    : video_decoder_client_receiver_(this),
      media_log_receiver_(this),
      stable_video_frame_handle_releaser_receiver_(this),
      dst_video_decoder_(std::move(dst_video_decoder)),
      dst_video_decoder_receiver_(dst_video_decoder_.get()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!dst_video_decoder_);
  dst_video_decoder_remote_.Bind(
      dst_video_decoder_receiver_.BindNewPipeAndPassRemote());
}

StableVideoDecoderService::~StableVideoDecoderService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StableVideoDecoderService::GetSupportedConfigs(
    GetSupportedConfigsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
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
    std::move(callback).Run(DecoderStatus::Codes::kFailedToCreateDecoder,
                            /*needs_bitstream_conversion=*/false,
                            /*max_decode_requests=*/1,
                            VideoDecoderType::kUnknown);
    return;
  }

  // The |config| should have been validated at deserialization time.
  DCHECK(config.IsValidConfig());

  // TODO(b/195769334): implement out-of-process video decoding of hardware
  // protected content.
  if (config.is_encrypted()) {
    std::move(callback).Run(DecoderStatus::Codes::kUnsupportedConfig,
                            /*needs_bitstream_conversion=*/false,
                            /*max_decode_requests=*/1,
                            VideoDecoderType::kUnknown);
    return;
  }
  dst_video_decoder_remote_->Initialize(
      config, low_delay, /*cdm_id=*/absl::nullopt, std::move(callback));
}

void StableVideoDecoderService::Decode(
    const scoped_refptr<DecoderBuffer>& buffer,
    DecodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void StableVideoDecoderService::Reset(ResetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void StableVideoDecoderService::ReleaseVideoFrame(
    const base::UnguessableToken& release_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
}

void StableVideoDecoderService::RequestOverlayInfo(
    bool restart_for_transitions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void StableVideoDecoderService::AddLogRecord(const MediaLogRecord& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

}  // namespace media
