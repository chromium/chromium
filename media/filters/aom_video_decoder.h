// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AOM_VIDEO_DECODER_H_
#define MEDIA_FILTERS_AOM_VIDEO_DECODER_H_

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"

struct aom_codec_ctx;
struct aom_image;

namespace media {
class FrameBufferPool;
class MediaLog;

// libaom video decoder wrapper.
class MEDIA_EXPORT AomVideoDecoder : public VideoDecoder {
 public:
  explicit AomVideoDecoder(MediaLog* media_log);
  ~AomVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(const base::Closure& reset_cb) override;

 private:
  enum class DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kError
  };

  // Releases any configured decoder and clears |aom_decoder_|.
  void CloseDecoder();

  // Invokes the decoder and calls |output_cb_| for any returned frames.
  bool DecodeBuffer(const DecoderBuffer* buffer);

  // Copies the contents of |img| into a new VideoFrame; attempts to reuse
  // previously allocated memory via |frame_pool_| for performance.
  scoped_refptr<VideoFrame> CopyImageToVideoFrame(const struct aom_image* img);

  THREAD_CHECKER(thread_checker_);

  // Used to report error messages to the client.
  MediaLog* const media_log_;

  // Current decoder state. Used to ensure methods are called as expected.
  DecoderState state_ = DecoderState::kUninitialized;

  // Callback given during Initialize() used for delivering decoded frames.
  OutputCB output_cb_;

  // The configuration passed to Initialize(), saved since some fields are
  // needed to annotate video frames after decoding.
  VideoDecoderConfig config_;

  // Pool used for memory efficiency when vending frames from the decoder.
  scoped_refptr<FrameBufferPool> memory_pool_;

  // Timestamps are FIFO for libaom decoding.
  base::circular_deque<base::TimeDelta> timestamps_;

  // The allocated decoder; null before Initialize() and anytime after
  // CloseDecoder().
  std::unique_ptr<aom_codec_ctx> aom_decoder_;

  DISALLOW_COPY_AND_ASSIGN(AomVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AOM_VIDEO_DECODER_H_
