// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_
#define MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/audio_decoder.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/sample_format.h"

struct OpusMSDecoder;

namespace media {

class AudioBufferMemoryPool;
class AudioDiscardHelper;
class DecoderBuffer;

struct OpusMSDecoderDeleter {
  void operator()(OpusMSDecoder* ptr) const;
};

class MEDIA_EXPORT OpusAudioDecoder : public AudioDecoder {
 public:
  enum class ExecutionMode { kAsynchronous, kSynchronous };

  explicit OpusAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      ExecutionMode mode = ExecutionMode::kAsynchronous);
  OpusAudioDecoder(const OpusAudioDecoder&) = delete;
  OpusAudioDecoder(OpusAudioDecoder&&) = delete;
  OpusAudioDecoder& operator=(const OpusAudioDecoder&) = delete;
  OpusAudioDecoder& operator=(OpusAudioDecoder&&) = delete;
  ~OpusAudioDecoder() override;

  // AudioDecoder implementation.
  AudioDecoderType GetDecoderType() const override;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;

 private:
  bool DecodeBuffer(const scoped_refptr<DecoderBuffer>& input);

  bool ConfigureDecoder();
  void ResetTimestampState();

  // If the execution mode is set to asynchronous, wraps the `callback` in a
  // bind post task on the current default task runner. Otherwise, a noop.
  template <typename T>
  std::decay_t<T> BindCallbackIfNeeded(T&& callback) {
    return mode_ == ExecutionMode::kAsynchronous
               ? base::BindPostTask(task_runner_, std::forward<T>(callback))
               : std::forward<T>(callback);
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const ExecutionMode mode_ = ExecutionMode::kAsynchronous;

  AudioDecoderConfig config_;
  OutputCB output_cb_;
  std::unique_ptr<OpusMSDecoder, OpusMSDecoderDeleter> opus_decoder_;
  std::unique_ptr<AudioDiscardHelper> discard_helper_;

  scoped_refptr<AudioBufferMemoryPool> pool_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_
