// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/offloading_video_encoder.h"

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "media/base/video_frame.h"
#include "media/video/video_encoder_info.h"

namespace media {

OffloadingVideoEncoder::OffloadingVideoEncoder(
    std::unique_ptr<VideoEncoder> wrapped_encoder,
    const scoped_refptr<base::SequencedTaskRunner> work_runner,
    const scoped_refptr<base::SequencedTaskRunner> callback_runner)
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

OffloadingVideoEncoder::OffloadingVideoEncoder(
    std::unique_ptr<VideoEncoder> wrapped_encoder)
    : OffloadingVideoEncoder(
          std::move(wrapped_encoder),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::USER_BLOCKING,
               base::WithBaseSyncPrimitives(), base::MayBlock()}),
          base::SequencedTaskRunner::GetCurrentDefault()) {}

void OffloadingVideoEncoder::Initialize(VideoCodecProfile profile,
                                        const Options& options,
                                        EncoderInfoCB info_cb,
                                        OutputCB output_cb,
                                        EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncoder::Initialize,
                     base::Unretained(wrapped_encoder_.get()), profile, options,
                     WrapCallback(std::move(info_cb)),
                     WrapCallback(std::move(output_cb)),
                     WrapCallback(std::move(done_cb))));
}

void OffloadingVideoEncoder::Encode(scoped_refptr<VideoFrame> frame,
                                    const EncodeOptions& encode_options,
                                    EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media", "OffloadingVideoEncoder::Encode");
  work_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncoder::Encode,
                     base::Unretained(wrapped_encoder_.get()), std::move(frame),
                     encode_options, WrapCallback(std::move(done_cb))));
}

void OffloadingVideoEncoder::ChangeOptions(const Options& options,
                                           OutputCB output_cb,
                                           EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoder::ChangeOptions,
                                base::Unretained(wrapped_encoder_.get()),
                                options, WrapCallback(std::move(output_cb)),
                                WrapCallback(std::move(done_cb))));
}

void OffloadingVideoEncoder::Flush(EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoder::Flush,
                                base::Unretained(wrapped_encoder_.get()),
                                WrapCallback(std::move(done_cb))));
}

OffloadingVideoEncoder::~OffloadingVideoEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  work_runner_->DeleteSoon(FROM_HERE, std::move(wrapped_encoder_));
}

template <class T>
T OffloadingVideoEncoder::WrapCallback(T cb) {
  DCHECK(callback_runner_);
  return base::BindPostTask(callback_runner_, std::move(cb));
}

}  // namespace media
