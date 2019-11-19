// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DAV1D_VIDEO_DECODER_H_
#define MEDIA_FILTERS_DAV1D_VIDEO_DECODER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "media/filters/offloading_video_decoder.h"

struct Dav1dContext;
struct Dav1dPicture;

namespace media {
class MediaLog;

class MEDIA_EXPORT Dav1dVideoDecoder : public OffloadableVideoDecoder {
 public:
  Dav1dVideoDecoder(MediaLog* media_log,
                    OffloadState offload_state = OffloadState::kNormal);
  ~Dav1dVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
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

 private:
  enum class DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kError
  };

  // Releases any configured decoder and clears |dav1d_decoder_|.
  void CloseDecoder();

  // Invokes the decoder and calls |output_cb_| for any returned frames.
  bool DecodeBuffer(scoped_refptr<DecoderBuffer> buffer);

  scoped_refptr<VideoFrame> CopyImageToVideoFrame(const Dav1dPicture* img);

  // Used to report error messages to the client.
  MediaLog* const media_log_ = nullptr;

  // Indicates if the decoder is being wrapped by OffloadVideoDecoder; controls
  // whether callbacks are bound to the current loop on calls.
  const bool bind_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Current decoder state. Used to ensure methods are called as expected.
  DecoderState state_ = DecoderState::kUninitialized;

  // Callback given during Initialize() used for delivering decoded frames.
  OutputCB output_cb_;

  // The configuration passed to Initialize(), saved since some fields are
  // needed to annotate video frames after decoding.
  VideoDecoderConfig config_;

  // The allocated decoder; null before Initialize() and anytime after
  // CloseDecoder().
  Dav1dContext* dav1d_decoder_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Dav1dVideoDecoder);
};

// Helper class for creating a Dav1dVideoDecoder which will offload all AV1
// content from the media thread.
class OffloadingDav1dVideoDecoder : public OffloadingVideoDecoder {
 public:
  explicit OffloadingDav1dVideoDecoder(MediaLog* media_log)
      : OffloadingVideoDecoder(
            0,
            std::vector<VideoCodec>(1, kCodecAV1),
            std::make_unique<Dav1dVideoDecoder>(
                media_log,
                OffloadableVideoDecoder::OffloadState::kOffloaded)) {}
};

}  // namespace media

#endif  // MEDIA_FILTERS_DAV1D_VIDEO_DECODER_H_
