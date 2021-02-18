// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_GAV1_VIDEO_DECODER_H_
#define MEDIA_FILTERS_GAV1_VIDEO_DECODER_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/base/media_export.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame_pool.h"
#include "media/filters/offloading_video_decoder.h"

namespace libgav1 {
class Decoder;
}  // namespace libgav1

namespace media {
class MediaLog;

class MEDIA_EXPORT Gav1VideoDecoder : public OffloadableVideoDecoder {
 public:
  static SupportedVideoDecoderConfigs SupportedConfigs();

  explicit Gav1VideoDecoder(MediaLog* media_log,
                            OffloadState offload_state = OffloadState::kNormal);
  ~Gav1VideoDecoder() override;
  Gav1VideoDecoder(const Gav1VideoDecoder&) = delete;
  Gav1VideoDecoder& operator=(const Gav1VideoDecoder&) = delete;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  VideoDecoderType GetDecoderType() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;

  // OffloadableVideoDecoder implementation.
  void Detach() override;

  scoped_refptr<VideoFrame> CreateVideoFrame(VideoPixelFormat format,
                                             const gfx::Size& coded_size,
                                             const gfx::Rect& visible_rect);

 private:
  enum class DecoderState {
    kUninitialized,
    kDecoding,
    kError,
  };

  void CloseDecoder();

  // Invokes the decoder and calls |output_cb_| for any returned frames.
  bool DecodeBuffer(scoped_refptr<DecoderBuffer> buffer);

  // Used to report error messages to the client.
  MediaLog* const media_log_;
  const bool bind_callbacks_;

  // Info configured in Initialize(). These are used in outputting frames.
  VideoColorSpace color_space_;
  gfx::Size natural_size_;

  DecoderState state_ = DecoderState::kUninitialized;

  // A decoded buffer used in libgav1 is allocated and managed by
  // |frame_pool_|. The buffer can be reused only if libgav1's decoder doesn't
  // use the buffer and rendering the frame is complete.
  VideoFramePool frame_pool_;

  OutputCB output_cb_;
  std::unique_ptr<libgav1::Decoder> libgav1_decoder_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Helper class for creating a Gav1VideoDecoder which will offload all AV1
// content from the media thread.
class OffloadingGav1VideoDecoder : public OffloadingVideoDecoder {
 public:
  explicit OffloadingGav1VideoDecoder(MediaLog* media_log)
      : OffloadingVideoDecoder(
            0,
            std::vector<VideoCodec>(1, kCodecAV1),
            std::make_unique<Gav1VideoDecoder>(
                media_log,
                OffloadableVideoDecoder::OffloadState::kOffloaded)) {}
};
}  // namespace media
#endif  // MEDIA_FILTERS_GAV1_VIDEO_DECODER_H_
