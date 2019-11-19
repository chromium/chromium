// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/offloading_video_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/post_task.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"

namespace media {

// Helper class which manages cancellation of Decode() after Reset() and makes
// it easier to destruct on the proper thread.
class CancellationHelper {
 public:
  CancellationHelper(std::unique_ptr<OffloadableVideoDecoder> decoder)
      : cancellation_flag_(std::make_unique<base::AtomicFlag>()),
        decoder_(std::move(decoder)) {}

  // Safe to call from any thread.
  void Cancel() { cancellation_flag_->Set(); }

  void Decode(scoped_refptr<DecoderBuffer> buffer,
              VideoDecoder::DecodeCB decode_cb) {
    if (cancellation_flag_->IsSet()) {
      std::move(decode_cb).Run(DecodeStatus::ABORTED);
      return;
    }

    decoder_->Decode(std::move(buffer), std::move(decode_cb));
  }

  void Reset(base::OnceClosure reset_cb) {
    // OffloadableVideoDecoders are required to have a synchronous Reset(), so
    // we don't need to wait for the Reset to complete. Despite this, we don't
    // want to run |reset_cb| before we've reset the cancellation flag or the
    // client may end up issuing another Reset() before this code runs.
    decoder_->Reset(base::DoNothing());
    cancellation_flag_.reset(new base::AtomicFlag());
    std::move(reset_cb).Run();
  }

  OffloadableVideoDecoder* decoder() const { return decoder_.get(); }

 private:
  std::unique_ptr<base::AtomicFlag> cancellation_flag_;
  std::unique_ptr<OffloadableVideoDecoder> decoder_;

  DISALLOW_COPY_AND_ASSIGN(CancellationHelper);
};

OffloadingVideoDecoder::OffloadingVideoDecoder(
    int min_offloading_width,
    std::vector<VideoCodec> supported_codecs,
    std::unique_ptr<OffloadableVideoDecoder> decoder)
    : min_offloading_width_(min_offloading_width),
      supported_codecs_(std::move(supported_codecs)),
      helper_(std::make_unique<CancellationHelper>(std::move(decoder))) {
  DETACH_FROM_THREAD(thread_checker_);
}

OffloadingVideoDecoder::~OffloadingVideoDecoder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // The |helper_| must always be destroyed on the |offload_task_runner_| since
  // we may still have tasks posted to it.
  if (offload_task_runner_)
    offload_task_runner_->DeleteSoon(FROM_HERE, std::move(helper_));
}

std::string OffloadingVideoDecoder::GetDisplayName() const {
  // This call is expected to be static and safe to call from any thread.
  return helper_->decoder()->GetDisplayName();
}

void OffloadingVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                        bool low_delay,
                                        CdmContext* cdm_context,
                                        InitCB init_cb,
                                        const OutputCB& output_cb,
                                        const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(config.IsValidConfig());

  const bool disable_offloading =
      config.is_encrypted() ||
      config.coded_size().width() < min_offloading_width_ ||
      std::find(supported_codecs_.begin(), supported_codecs_.end(),
                config.codec()) == supported_codecs_.end();

  if (initialized_) {
    initialized_ = false;

    // We're transitioning from offloading to no offloading, so detach from the
    // offloading thread so we can run on the media thread.
    if (disable_offloading && offload_task_runner_) {
      offload_task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&OffloadableVideoDecoder::Detach,
                         base::Unretained(helper_->decoder())),
          // We must trampoline back trough OffloadingVideoDecoder because it's
          // possible for this class to be destroyed during Initialize().
          base::BindOnce(&OffloadingVideoDecoder::Initialize,
                         weak_factory_.GetWeakPtr(), config, low_delay,
                         cdm_context, std::move(init_cb), output_cb,
                         waiting_cb));
      return;
    }

    // We're transitioning from no offloading to offloading, so detach from the
    // media thread so we can run on the offloading thread.
    if (!disable_offloading && !offload_task_runner_)
      helper_->decoder()->Detach();
  }

  DCHECK(!initialized_);
  initialized_ = true;

  // Offloaded decoders expect asynchronous execution of callbacks; even if we
  // aren't currently using the offload thread.
  InitCB bound_init_cb = BindToCurrentLoop(std::move(init_cb));
  OutputCB bound_output_cb = BindToCurrentLoop(output_cb);

  // If we're not offloading just pass through to the wrapped decoder.
  if (disable_offloading) {
    offload_task_runner_ = nullptr;
    helper_->decoder()->Initialize(config, low_delay, cdm_context,
                                   std::move(bound_init_cb), bound_output_cb,
                                   waiting_cb);
    return;
  }

  if (!offload_task_runner_) {
    offload_task_runner_ = base::CreateSequencedTaskRunner(
        {base::ThreadPool(), base::TaskPriority::USER_BLOCKING});
  }

  offload_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OffloadableVideoDecoder::Initialize,
                     base::Unretained(helper_->decoder()), config, low_delay,
                     cdm_context, std::move(bound_init_cb), bound_output_cb,
                     waiting_cb));
}

void OffloadingVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                    DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buffer);
  DCHECK(decode_cb);

  DecodeCB bound_decode_cb = BindToCurrentLoop(std::move(decode_cb));
  if (!offload_task_runner_) {
    helper_->decoder()->Decode(std::move(buffer), std::move(bound_decode_cb));
    return;
  }

  offload_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CancellationHelper::Decode,
                                base::Unretained(helper_.get()),
                                std::move(buffer), std::move(bound_decode_cb)));
}

void OffloadingVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::OnceClosure bound_reset_cb = BindToCurrentLoop(std::move(reset_cb));
  if (!offload_task_runner_) {
    helper_->Reset(std::move(bound_reset_cb));
  } else {
    helper_->Cancel();
    offload_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CancellationHelper::Reset,
                                  base::Unretained(helper_.get()),
                                  std::move(bound_reset_cb)));
  }
}

int OffloadingVideoDecoder::GetMaxDecodeRequests() const {
  // If we're offloading, try to parallelize decodes as well. Take care when
  // adjusting this number as it may dramatically increase memory usage and
  // reduce seek times. See http://crbug.com/731841.
  //
  // The current value of 2 was determined via experimental adjustment until a
  // 4K60 VP9 playback dropped zero frames.
  return offload_task_runner_ ? 2 : 1;
}

}  // namespace media
