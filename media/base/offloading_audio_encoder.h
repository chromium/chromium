// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_OFFLOADING_AUDIO_ENCODER_H_
#define MEDIA_BASE_OFFLOADING_AUDIO_ENCODER_H_

#include <memory>
#include <type_traits>

#include "base/sequence_checker.h"
#include "media/base/audio_encoder.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

// A wrapper around audio encoder that offloads all the calls to a dedicated
// task runner. It's used to move synchronous software encoding work off the
// current (main) thread.
class MEDIA_EXPORT OffloadingAudioEncoder final : public AudioEncoder {
 public:
  // |work_runner| - task runner for encoding work
  // |callback_runner| - all encoder's callbacks will be executed on this task
  // runner.
  OffloadingAudioEncoder(
      std::unique_ptr<AudioEncoder> wrapped_encoder,
      scoped_refptr<base::SequencedTaskRunner> work_runner,
      scoped_refptr<base::SequencedTaskRunner> callback_runner);

  // Uses current task runner for callbacks and asks thread pool for a new task
  // runner to do actual encoding work.
  explicit OffloadingAudioEncoder(
      std::unique_ptr<AudioEncoder> wrapped_encoder);

  ~OffloadingAudioEncoder() override;

  void Initialize(const Options& options,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override;

  void Encode(std::unique_ptr<AudioBus> audio_bus,
              base::TimeTicks capture_time,
              EncoderStatusCB done_cb) override;

  void Flush(EncoderStatusCB done_cb) override;

 private:
  template <class T>
  T WrapCallback(T cb);

  std::unique_ptr<AudioEncoder> wrapped_encoder_;
  const scoped_refptr<base::SequencedTaskRunner> work_runner_;
  const scoped_refptr<base::SequencedTaskRunner> callback_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_BASE_OFFLOADING_AUDIO_ENCODER_H_
