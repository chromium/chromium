// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include <list>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame_pool.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVCodecContext;
struct AVFrame;

namespace media {

class DecoderBuffer;
class FFmpegDecodingLoop;
class MediaLog;

class MEDIA_EXPORT FFmpegVideoDecoder : public VideoDecoder {
 public:
  static bool IsCodecSupported(VideoCodec codec);

  explicit FFmpegVideoDecoder(MediaLog* media_log);
  ~FFmpegVideoDecoder() override;

  // Allow decoding of individual NALU. Entire frames are required by default.
  // Disables low-latency mode. Must be called before Initialize().
  void set_decode_nalus(bool decode_nalus) { decode_nalus_ = decode_nalus; }

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;

  // Callback called from within FFmpeg to allocate a buffer based on
  // the dimensions of |codec_context|. See AVCodecContext.get_buffer2
  // documentation inside FFmpeg.
  int GetVideoBuffer(struct AVCodecContext* codec_context,
                     AVFrame* frame,
                     int flags);

 private:
  enum DecoderState {
    kUninitialized,
    kNormal,
    kDecodeFinished,
    kError
  };

  // Handles decoding of an unencrypted encoded buffer. A return value of false
  // indicates that an error has occurred.
  bool FFmpegDecode(const DecoderBuffer& buffer);
  bool OnNewFrame(AVFrame* frame);

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns true if initialization was successful.
  bool ConfigureDecoder(const VideoDecoderConfig& config, bool low_delay);

  // Releases resources associated with |codec_context_|.
  void ReleaseFFmpegResources();

  base::ThreadChecker thread_checker_;
  MediaLog* media_log_;

  DecoderState state_;

  OutputCB output_cb_;

  // FFmpeg structures owned by this object.
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;

  VideoDecoderConfig config_;

  VideoFramePool frame_pool_;

  bool decode_nalus_;

  std::unique_ptr<FFmpegDecodingLoop> decoding_loop_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
