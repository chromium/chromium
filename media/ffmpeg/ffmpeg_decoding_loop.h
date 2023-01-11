// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FFMPEG_FFMPEG_DECODING_LOOP_H_
#define MEDIA_FFMPEG_FFMPEG_DECODING_LOOP_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVCodecContext;
struct AVFrame;
struct AVPacket;

namespace media {

class MEDIA_EXPORT FFmpegDecodingLoop {
 public:
  enum class DecodeStatus {
    // Everything went just okay.
    kOkay,

    // Indicates that avcodec_send_packet() failed on the current packet. This
    // error is fatal and indicates the decoding loop is no longer viable.
    kSendPacketFailed,

    // Indicates that avcodec_receive_frame() failed on some packet; it may be a
    // packet sent in the past. If |continue_on_decoding_errors| is true, this
    // code is recoverable and may be ignored.
    kDecodeFrameFailed,

    // Returned when FrameReadyCB returns false which indicates that an internal
    // error has occurred; will immediately stop the decoding loop. This should
    // not be considered recoverable since internal loop state is unknown.
    kFrameProcessingFailed,
  };

  // Creates a decoding loop using the already initialized codec |context|. If
  // decoding errors should be non-fatal, set |continue_on_decoding_errors| to
  // true; note: send packet failures are always fatal.
  FFmpegDecodingLoop(AVCodecContext* context,
                     bool continue_on_decoding_errors = false);

  FFmpegDecodingLoop(const FFmpegDecodingLoop&) = delete;
  FFmpegDecodingLoop& operator=(const FFmpegDecodingLoop&) = delete;

  ~FFmpegDecodingLoop();

  // Callback issued when the decoding loop has produced a frame. |frame| is
  // owned by the decoding loop. Return true to continue the decoding loop.
  using FrameReadyCB = base::RepeatingCallback<bool(AVFrame* frame)>;

  // Spins a generic decoding which decodes all available frames and sends them
  // to |frame_ready_cb| given a single input |packet|. Returns an enum with
  // success or the appropriate error code if failure.
  //
  // If |packet| is an end of stream packet all available frames still in the
  // decoder will be returned. After end of stream, |context| will not be usable
  // for decoding until avcodec_flush_buffers() is called on the context; which
  // the decoding loop does not handle.
  DecodeStatus DecodePacket(const AVPacket* packet,
                            FrameReadyCB frame_ready_cb);

  int last_averror_code() const { return last_averror_code_; }

 private:
  const bool continue_on_decoding_errors_;
  const raw_ptr<AVCodecContext> context_;
  std::unique_ptr<AVFrame, ScopedPtrAVFreeFrame> frame_;
  int last_averror_code_ = 0;
};

}  // namespace media

#endif  // MEDIA_FFMPEG_FFMPEG_DECODING_LOOP_H_
