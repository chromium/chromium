// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_

#include <list>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/sample_format.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVCodecContext;
struct AVFrame;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioDiscardHelper;
class DecoderBuffer;
class FFmpegDecodingLoop;

class MEDIA_EXPORT FFmpegAudioDecoder : public AudioDecoder {
 public:
  FFmpegAudioDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      MediaLog* media_log);
  ~FFmpegAudioDecoder() override;

  // AudioDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              const DecodeCB& decode_cb) override;
  void Reset(base::OnceClosure closure) override;

  // Callback called from within FFmpeg to allocate a buffer based on the
  // properties of |codec_context| and |frame|. See AVCodecContext.get_buffer2
  // documentation inside FFmpeg.
  int GetAudioBuffer(struct AVCodecContext* s, AVFrame* frame, int flags);

 private:
  // There are four states the decoder can be in:
  //
  // - kUninitialized: The decoder is not initialized.
  // - kNormal: This is the normal state. The decoder is idle and ready to
  //            decode input buffers, or is decoding an input buffer.
  // - kDecodeFinished: EOS buffer received, codec flushed and decode finished.
  //                    No further Decode() call should be made.
  // - kError: Unexpected error happened.
  //
  // These are the possible state transitions.
  //
  // kUninitialized -> kNormal:
  //     The decoder is successfully initialized and is ready to decode buffers.
  // kNormal -> kDecodeFinished:
  //     When buffer->end_of_stream() is true.
  // kNormal -> kError:
  //     A decoding error occurs and decoding needs to stop.
  // (any state) -> kNormal:
  //     Any time Reset() is called.
  enum DecoderState {
    kUninitialized,
    kNormal,
    kDecodeFinished,
    kError
  };

  // Reset decoder and call |reset_cb_|.
  void DoReset();

  // Handles decoding an unencrypted encoded buffer.
  void DecodeBuffer(const DecoderBuffer& buffer, const DecodeCB& decode_cb);
  bool FFmpegDecode(const DecoderBuffer& buffer);
  bool OnNewFrame(const DecoderBuffer& buffer,
                  bool* decoded_frame_this_loop,
                  AVFrame* frame);

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns true if initialization was successful.
  bool ConfigureDecoder(const AudioDecoderConfig& config);

  // Releases resources associated with |codec_context_| .
  void ReleaseFFmpegResources();

  void ResetTimestampState(const AudioDecoderConfig& config);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  OutputCB output_cb_;

  DecoderState state_;

  // FFmpeg structures owned by this object.
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;

  AudioDecoderConfig config_;

  // AVSampleFormat initially requested; not Chrome's SampleFormat.
  int av_sample_format_;

  std::unique_ptr<AudioDiscardHelper> discard_helper_;

  MediaLog* media_log_;

  scoped_refptr<AudioBufferMemoryPool> pool_;

  std::unique_ptr<FFmpegDecodingLoop> decoding_loop_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FFmpegAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
