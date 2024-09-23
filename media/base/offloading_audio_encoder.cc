// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/offloading_audio_encoder.h"

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace media {

OffloadingAudioEncoder::OffloadingAudioEncoder(
    std::unique_ptr<AudioEncoder> wrapped_encoder,
    scoped_refptr<base::SequencedTaskRunner> work_runner,
    scoped_refptr<base::SequencedTaskRunner> callback_runner)
    : wrapped_encoder_(std::move(wrapped_encoder)),
      work_runner_(std::move(work_runner)),
      callback_runner_(std::move(callback_runner)) {
  DCHECK(wrapped_encoder_);
  DCHECK(work_runner_);
  DCHECK(callback_runner_);
  DCHECK_NE(callback_runner_, work_runner_);

  // Tell the inner encoder not to bother wrapping callbacks into separate
  // runner tasks and call them directly.
  wrapped_encoder_->DisablePostedCallbacks();
}

OffloadingAudioEncoder::OffloadingAudioEncoder(
    std::unique_ptr<AudioEncoder> wrapped_encoder)
    : OffloadingAudioEncoder(std::move(wrapped_encoder),
                             base::ThreadPool::CreateSequencedTaskRunner(
                                 {base::TaskPriority::USER_BLOCKING}),
                             base::SequencedTaskRunner::GetCurrentDefault()) {}

void OffloadingAudioEncoder::Initialize(const Options& options,
                                        OutputCB output_cb,
                                        EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioEncoder::Initialize,
                                base::Unretained(wrapped_encoder_.get()),
                                options, WrapCallback(std::move(output_cb)),
                                WrapCallback(std::move(done_cb))));
}

void OffloadingAudioEncoder::Encode(std::unique_ptr<AudioBus> audio_bus,
                                    base::TimeTicks capture_time,
                                    EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioEncoder::Encode,
                                base::Unretained(wrapped_encoder_.get()),
                                std::move(audio_bus), capture_time,
                                WrapCallback(std::move(done_cb))));
}

void OffloadingAudioEncoder::Flush(EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioEncoder::Flush,
                                base::Unretained(wrapped_encoder_.get()),
                                WrapCallback(std::move(done_cb))));
}

OffloadingAudioEncoder::~OffloadingAudioEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->DeleteSoon(FROM_HERE, std::move(wrapped_encoder_));
}

template <class T>
T OffloadingAudioEncoder::WrapCallback(T cb) {
  DCHECK(callback_runner_);
  return base::BindPostTask(callback_runner_, std::move(cb));
}

}  // namespace media
