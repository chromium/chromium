// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_stateless_video_decoder.h"

#include "base/notreached.h"

namespace media {

// static
std::unique_ptr<VideoDecoderMixin> V4L2StatelessVideoDecoder::Create(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  return base::WrapUnique<VideoDecoderMixin>(new V4L2StatelessVideoDecoder(
      std::move(media_log), std::move(decoder_task_runner), std::move(client)));
}

V4L2StatelessVideoDecoder::V4L2StatelessVideoDecoder(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client)
    : VideoDecoderMixin(std::move(media_log),
                        std::move(decoder_task_runner),
                        std::move(client)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
}

V4L2StatelessVideoDecoder::~V4L2StatelessVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
}

// static
absl::optional<SupportedVideoDecoderConfigs>
V4L2StatelessVideoDecoder::GetSupportedConfigs() {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

void V4L2StatelessVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                           bool low_delay,
                                           CdmContext* cdm_context,
                                           InitCB init_cb,
                                           const OutputCB& output_cb,
                                           const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

void V4L2StatelessVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                       DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

void V4L2StatelessVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

bool V4L2StatelessVideoDecoder::NeedsBitstreamConversion() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return false;
}

bool V4L2StatelessVideoDecoder::CanReadWithoutStalling() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return false;
}

int V4L2StatelessVideoDecoder::GetMaxDecodeRequests() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return -1;
}

VideoDecoderType V4L2StatelessVideoDecoder::GetDecoderType() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return VideoDecoderType::kV4L2;
}

bool V4L2StatelessVideoDecoder::IsPlatformDecoder() const {
  NOTREACHED() << "Our only owner VideoDecoderPipeline never calls here";
  return true;
}

void V4L2StatelessVideoDecoder::ApplyResolutionChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

size_t V4L2StatelessVideoDecoder::GetMaxOutputFramePoolSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
  return 0;
}

scoped_refptr<V4L2DecodeSurface> V4L2StatelessVideoDecoder::CreateSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

bool V4L2StatelessVideoDecoder::SubmitSlice(V4L2DecodeSurface* dec_surface,
                                            const uint8_t* data,
                                            size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
  return false;
}

void V4L2StatelessVideoDecoder::DecodeSurface(
    scoped_refptr<V4L2DecodeSurface> dec_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

void V4L2StatelessVideoDecoder::SurfaceReady(
    scoped_refptr<V4L2DecodeSurface> dec_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect,
    const VideoColorSpace& color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  NOTIMPLEMENTED();
}

}  // namespace media
