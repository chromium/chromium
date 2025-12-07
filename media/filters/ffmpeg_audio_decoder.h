// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_

#include <memory>
#include <type_traits>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/sample_format.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVCodecContext;
struct AVFrame;

namespace base {
class SequencedTaskRunner;
}

namespace media {

class AudioDiscardHelper;
class DecoderBuffer;
class FFmpegDecodingLoop;

class MEDIA_EXPORT FFmpegAudioDecoder : public AudioDecoder {
 public:
  enum class ExecutionMode { kAsynchronous, kSynchronous };

  FFmpegAudioDecoder(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      MediaLog* media_log,
      ExecutionMode mode = ExecutionMode::kAsynchronous);

  FFmpegAudioDecoder(const FFmpegAudioDecoder&) = delete;
  FFmpegAudioDecoder& operator=(const FFmpegAudioDecoder&) = delete;
  FFmpegAudioDecoder() = delete;

  ~FFmpegAudioDecoder() override;

  // AudioDecoder implementation.
  AudioDecoderType GetDecoderType() const override;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
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
  enum class DecoderState { kUninitialized, kNormal, kDecodeFinished, kError };

  // Reset decoder and call |reset_cb_|.
  void DoReset();

  // Handles decoding an unencrypted encoded buffer.
  void DecodeBuffer(const DecoderBuffer& buffer, DecodeCB decode_cb);
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

  // If the execution mode is set to asynchronous, wraps the `callback` in a
  // bind post task on the current default task runner. Otherwise, a noop.
  template <typename T>
  std::decay_t<T> BindCallbackIfNeeded(T&& callback) {
    return mode_ == ExecutionMode::kAsynchronous
               ? base::BindPostTask(task_runner_, std::forward<T>(callback))
               : std::forward<T>(callback);
  }

  // NOTE: the `task_runner_` is allowed to be nullptr only if the execution
  // mode is synchronous.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The media log is a required field. A NullMediaLog may be passed but
  // not nullptr.
  raw_ptr<MediaLog, DanglingUntriaged> media_log_ = nullptr;

  // The threading mode that this decoder should operate in.
  const ExecutionMode mode_ = ExecutionMode::kAsynchronous;

  // Callback used to deliver frames, set on initialization.
  OutputCB output_cb_;

  SEQUENCE_CHECKER(sequence_checker_);

  DecoderState state_ = DecoderState::kUninitialized;

  // FFmpeg structures owned by this object.
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;

  AudioDecoderConfig config_;

  // AVSampleFormat initially requested; not Chrome's SampleFormat.
  int av_sample_format_ = 0;

  std::unique_ptr<AudioDiscardHelper> discard_helper_;

  scoped_refptr<AudioBufferMemoryPool> pool_;

  std::unique_ptr<FFmpegDecodingLoop> decoding_loop_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
