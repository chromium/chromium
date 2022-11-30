// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_thumbnail_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"

namespace media {

VideoThumbnailDecoder::VideoThumbnailDecoder(
    std::unique_ptr<VideoDecoder> decoder,
    const VideoDecoderConfig& config,
    std::vector<uint8_t> encoded_data)
    : decoder_(std::move(decoder)),
      config_(config),
      encoded_data_(std::move(encoded_data)) {
  DCHECK(!encoded_data_.empty());
  DCHECK(config_.IsValidConfig());
}

VideoThumbnailDecoder::~VideoThumbnailDecoder() = default;

void VideoThumbnailDecoder::Start(VideoFrameCallback video_frame_callback) {
  video_frame_callback_ = std::move(video_frame_callback);
  DCHECK(video_frame_callback_);
  decoder_->Initialize(
      config_, false, nullptr,
      base::BindOnce(&VideoThumbnailDecoder::OnVideoDecoderInitialized,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&VideoThumbnailDecoder::OnVideoFrameDecoded,
                          weak_factory_.GetWeakPtr()),
      base::DoNothing());
}

void VideoThumbnailDecoder::OnVideoDecoderInitialized(DecoderStatus status) {
  if (!status.is_ok()) {
    NotifyComplete(nullptr);
    return;
  }

  auto buffer =
      DecoderBuffer::CopyFrom(&encoded_data_[0], encoded_data_.size());
  encoded_data_.clear();
  decoder_->Decode(buffer,
                   base::BindOnce(&VideoThumbnailDecoder::OnVideoBufferDecoded,
                                  weak_factory_.GetWeakPtr()));
}

void VideoThumbnailDecoder::OnVideoBufferDecoded(DecoderStatus status) {
  if (!status.is_ok()) {
    NotifyComplete(nullptr);
    return;
  }

  // Enqueue eos since only one video frame is needed for thumbnail.
  decoder_->Decode(DecoderBuffer::CreateEOSBuffer(),
                   base::BindOnce(&VideoThumbnailDecoder::OnEosBufferDecoded,
                                  weak_factory_.GetWeakPtr()));
}

void VideoThumbnailDecoder::OnEosBufferDecoded(DecoderStatus status) {
  if (!status.is_ok())
    NotifyComplete(nullptr);
}

void VideoThumbnailDecoder::OnVideoFrameDecoded(
    scoped_refptr<VideoFrame> frame) {
  NotifyComplete(std::move(frame));
}

void VideoThumbnailDecoder::NotifyComplete(scoped_refptr<VideoFrame> frame) {
  DCHECK(video_frame_callback_);
  std::move(video_frame_callback_).Run(std::move(frame));
}

}  // namespace media
