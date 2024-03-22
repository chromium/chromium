// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_

#include <memory>

#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/types/id_type.h"
#include "media/base/frame_buffer_pool.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
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

  FFmpegVideoDecoder(const FFmpegVideoDecoder&) = delete;
  FFmpegVideoDecoder& operator=(const FFmpegVideoDecoder&) = delete;

  ~FFmpegVideoDecoder() override;

  // Allow decoding of individual NALU. Entire frames are required by default.
  // Disables low-latency mode. Must be called before Initialize().
  void set_decode_nalus(bool decode_nalus) { decode_nalus_ = decode_nalus; }

  // VideoDecoder implementation.
  VideoDecoderType GetDecoderType() const override;
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

  void force_allocation_error_for_testing() { force_allocation_error_ = true; }

 private:
  enum class DecoderState { kUninitialized, kNormal, kDecodeFinished, kError };

  // Handles decoding of an unencrypted encoded buffer. A return value of false
  // indicates that an error has occurred.
  bool FFmpegDecode(const DecoderBuffer& buffer);
  bool OnNewFrame(AVFrame* frame);

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns true if initialization was successful.
  bool ConfigureDecoder(const VideoDecoderConfig& config, bool low_delay);

  // Releases resources associated with |codec_context_|.
  void ReleaseFFmpegResources();

  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<MediaLog, DanglingUntriaged> media_log_;

  DecoderState state_ = DecoderState::kUninitialized;

  OutputCB output_cb_;

  // FFmpeg structures owned by this object.
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;

  // The gist here is that timestamps need to be 64 bits to store microsecond
  // precision. A 32 bit integer would overflow at ~35 minutes at this level of
  // precision. We can't cast the timestamp to the void ptr object used by the
  // opaque field in ffmpeg then, because it would lose data on a 32 bit build.
  // However, we don't actually have 2^31 timestamped frames in a single
  // playback, so it's fine to use the 32 bit value as a key in a map which
  // contains the actual timestamps. Additionally, we've in the past set 128
  // outstanding frames for re-ordering as a limit for cross-thread decoding
  // tasks, so we'll do that here too with the LRU cache.
  using TimestampId = base::IdType<int64_t, size_t, 0>;

  TimestampId::Generator timestamp_id_generator_;
  base::LRUCache<TimestampId, int64_t> timestamp_map_;

  VideoDecoderConfig config_;

  scoped_refptr<FrameBufferPool> frame_pool_;

  bool decode_nalus_ = false;

  bool force_allocation_error_ = false;

  std::unique_ptr<FFmpegDecodingLoop> decoding_loop_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODER_H_
