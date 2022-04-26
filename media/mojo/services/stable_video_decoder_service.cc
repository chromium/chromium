// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_service.h"

namespace media {

StableVideoDecoderService::StableVideoDecoderService(
    std::unique_ptr<mojom::VideoDecoder> dst_video_decoder)
    : dst_video_decoder_(std::move(dst_video_decoder)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!!dst_video_decoder_);
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
  NOTIMPLEMENTED();
}

void StableVideoDecoderService::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    mojo::PendingRemote<stable::mojom::StableCdmContext> cdm_context,
    InitializeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
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

}  // namespace media
