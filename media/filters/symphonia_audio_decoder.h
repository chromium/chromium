// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SYMPHONIA_AUDIO_DECODER_H_
#define MEDIA_FILTERS_SYMPHONIA_AUDIO_DECODER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "build/build_config.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_status.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/sample_format.h"
#include "media/base/timestamp_constants.h"

#if !BUILDFLAG(ENABLE_SYMPHONIA)
#error "This file should only be included with Symphonia support enabled."
#endif

#include "media/filters/symphonia_glue.rs.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class AudioDiscardHelper;
class DecoderBuffer;

// SymphoniaAudioDecoder uses the Symphonia library (via Rust FFI) to decode
// audio streams.
class MEDIA_EXPORT SymphoniaAudioDecoder : public AudioDecoder {
 public:
  enum class ExecutionMode { kAsynchronous, kSynchronous };

  SymphoniaAudioDecoder() = delete;

  SymphoniaAudioDecoder(scoped_refptr<base::SequencedTaskRunner> task_runner,
                        MediaLog* media_log,
                        ExecutionMode mode = ExecutionMode::kAsynchronous);

  SymphoniaAudioDecoder(const SymphoniaAudioDecoder&) = delete;
  SymphoniaAudioDecoder& operator=(const SymphoniaAudioDecoder&) = delete;

  ~SymphoniaAudioDecoder() override;

  // AudioDecoder implementation.
  AudioDecoderType GetDecoderType() const override;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;

  // Returns true if the codec is currently supported by the
  // SymphoniaAudioDecoder.
  static bool IsCodecSupported(AudioCodec codec);

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

  // Internal method for decoding the buffer, if it is in a state where that is
  // appropriate.
  void DecodeBuffer(scoped_refptr<DecoderBuffer> buffer,
                    DecodeCB decode_cb_bound);

  // Passes the encoded buffer to the Symphonia decoder instance. Returns true
  // on success, false otherwise. May result in zero or more calls to
  // output_cb_.
  bool SymphoniaDecode(const DecoderBuffer& buffer);

  // Creates a media::AudioBuffer from the decoded SymphoniaAudioBuffer.
  scoped_refptr<AudioBuffer> ToMediaAudioBuffer(
      const SymphoniaAudioBuffer& symphonia_buffer,
      base::TimeDelta timestamp);

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns true if initialization was successful.
  bool ConfigureDecoder(const AudioDecoderConfig& config);

  // Releases resources associated with |symphonia_decoder_|.
  void ReleaseSymphoniaResources();

  // Resets the timestamp helper state.
  void ResetTimestampState(const AudioDecoderConfig& config);

  // If the execution mode is set to asynchronous, wraps the `callback` in a
  // bind post task on the current default task runner. Otherwise, a noop.
  template <typename T>
  std::decay_t<T> BindCallbackIfNeeded(T&& callback) {
    return mode_ == ExecutionMode::kAsynchronous
               ? base::BindPostTask(task_runner_, std::forward<T>(callback))
               : std::forward<T>(callback);
  }

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // MediaLog for reporting messages and properties.
  const raw_ptr<MediaLog> media_log_ = nullptr;

  // The threading mode that this decoder should operate in.
  const ExecutionMode mode_ = ExecutionMode::kAsynchronous;

  // Callback provided during Initialize() used for decoded audio output.
  OutputCB output_cb_;

  // Current state of the decoder.
  DecoderState state_ = DecoderState::kUninitialized;

  // Symphonia decoder instance owned by this object via CXX bridge.
  std::optional<rust::Box<media::SymphoniaDecoder>> symphonia_decoder_;

  // Current audio decoder configuration.
  AudioDecoderConfig config_;

  // Used to estimate timestamps for buffers missing timestamps.
  std::unique_ptr<AudioDiscardHelper> discard_helper_;

  // Memory pool for creating AudioBuffer objects.
  scoped_refptr<AudioBufferMemoryPool> pool_ =
      base::MakeRefCounted<AudioBufferMemoryPool>();

  // The timestamp of the first frame. Symphonia is configured to count in
  // microseconds with the first frame starting at zero.
  std::optional<base::TimeDelta> first_frame_timestamp_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_SYMPHONIA_AUDIO_DECODER_H_
