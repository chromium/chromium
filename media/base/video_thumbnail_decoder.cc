// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_thumbnail_decoder.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
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
  DCHECK(video_frame_callback);

  // Always post this task since NotifyComplete() can destruct this class and
  // `decoder_` may crash if destructed during OutputCB.
  video_frame_callback_ =
      base::BindPostTaskToCurrentDefault(std::move(video_frame_callback));

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

  auto buffer = DecoderBuffer::CopyFrom(encoded_data_);
  encoded_data_.clear();
  decoder_->Decode(buffer,
                   base::BindOnce(&VideoThumbnailDecoder::OnVideoBufferDecoded,
                                  weak_factory_.GetWeakPtr()));
}

void VideoThumbnailDecoder::OnVideoBufferDecoded(DecoderStatus status) {
  if (!video_frame_callback_) {
    // OutputCB may run before DecodeCB, so skip EOS handling if so.
    return;
  }

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
  if (!video_frame_callback_) {
    // OutputCB may run before DecodeCB, so skip this step if so.
    return;
  }

  if (!status.is_ok()) {
    NotifyComplete(nullptr);
  }
}

void VideoThumbnailDecoder::OnVideoFrameDecoded(
    scoped_refptr<VideoFrame> frame) {
  // Some codecs may generate multiple outputs per input packet.
  if (video_frame_callback_) {
    NotifyComplete(std::move(frame));
    return;
  }
}

void VideoThumbnailDecoder::NotifyComplete(scoped_refptr<VideoFrame> frame) {
  DCHECK(video_frame_callback_);
  std::move(video_frame_callback_).Run(std::move(frame));
}

}  // namespace media
