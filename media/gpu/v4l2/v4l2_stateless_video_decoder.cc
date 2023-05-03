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
                        std::move(client)) {}

V4L2StatelessVideoDecoder::~V4L2StatelessVideoDecoder() {}

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
  NOTIMPLEMENTED();
}

void V4L2StatelessVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                       DecodeCB decode_cb) {
  NOTIMPLEMENTED();
}

void V4L2StatelessVideoDecoder::Reset(base::OnceClosure reset_cb) {
  NOTIMPLEMENTED();
}

bool V4L2StatelessVideoDecoder::NeedsBitstreamConversion() const {
  NOTIMPLEMENTED();
  return false;
}

bool V4L2StatelessVideoDecoder::CanReadWithoutStalling() const {
  NOTIMPLEMENTED();
  return false;
}

int V4L2StatelessVideoDecoder::GetMaxDecodeRequests() const {
  NOTIMPLEMENTED();
  return -1;
}

VideoDecoderType V4L2StatelessVideoDecoder::GetDecoderType() const {
  NOTIMPLEMENTED();
  return VideoDecoderType::kUnknown;
}

bool V4L2StatelessVideoDecoder::IsPlatformDecoder() const {
  NOTIMPLEMENTED();
  return false;
}

void V4L2StatelessVideoDecoder::ApplyResolutionChange() {
  NOTIMPLEMENTED();
}

size_t V4L2StatelessVideoDecoder::GetMaxOutputFramePoolSize() const {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace media
