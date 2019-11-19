// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_allocator.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/timestamp_constants.h"

namespace media {

namespace {

// This must be safe to call on any thread. Returns nullptr on failure.
std::unique_ptr<MediaCodecBridge> CreateMediaCodecInternal(
    CodecAllocator::CodecFactoryCB factory_cb,
    std::unique_ptr<VideoCodecConfig> codec_config) {
  TRACE_EVENT0("media", "CodecAllocator::CreateMediaCodec");
  base::ScopedBlockingCall scoped_block(FROM_HERE,
                                        base::BlockingType::MAY_BLOCK);
  return factory_cb.Run(*codec_config);
}

// Delete |codec| and signal |done_event| if it's not null.
void ReleaseMediaCodecInternal(std::unique_ptr<MediaCodecBridge> codec) {
  TRACE_EVENT0("media", "CodecAllocator::ReleaseMediaCodec");
  base::ScopedBlockingCall scoped_block(FROM_HERE,
                                        base::BlockingType::MAY_BLOCK);
  codec.reset();
}

scoped_refptr<base::SequencedTaskRunner> CreateCodecTaskRunner() {
  return base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

}  // namespace

// static
CodecAllocator* CodecAllocator::GetInstance(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  static base::NoDestructor<CodecAllocator> allocator(
      base::BindRepeating(&MediaCodecBridgeImpl::CreateVideoDecoder),
      task_runner);

  // Verify that this caller agrees on the task runner, if one was specified.
  DCHECK(!task_runner || allocator->task_runner_ == task_runner);
  return allocator.get();
}

void CodecAllocator::CreateMediaCodecAsync(
    CodecCreatedCB codec_created_cb,
    std::unique_ptr<VideoCodecConfig> codec_config) {
  DCHECK(codec_created_cb);
  DCHECK(codec_config);

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CodecAllocator::CreateMediaCodecAsync,
                       base::Unretained(this),
                       BindToCurrentLoop(std::move(codec_created_cb)),
                       std::move(codec_config)));
    return;
  }

  // Select the task runner before adding the PendingOperation and before
  // querying the |force_sw_codecs_| state.
  auto* task_runner = SelectCodecTaskRunner();

  // If we can't satisfy the request, fail the creation.
  if (codec_config->codec_type == CodecType::kSecure && force_sw_codecs_) {
    DLOG(ERROR) << "Secure software codec doesn't exist.";
    std::move(codec_created_cb).Run(nullptr);
    return;
  }

  if (force_sw_codecs_)
    codec_config->codec_type = CodecType::kSoftware;

  const auto start_time = tick_clock_->NowTicks();
  pending_operations_.push_back(start_time);

  // Post creation to the task runner. This may hang on broken platforms; if it
  // hangs, we will detect it on the next creation request, and future creations
  // will fallback to software.
  base::PostTaskAndReplyWithResult(
      std::move(task_runner), FROM_HERE,
      base::BindOnce(&CreateMediaCodecInternal, factory_cb_,
                     std::move(codec_config)),
      base::BindOnce(&CodecAllocator::OnCodecCreated, base::Unretained(this),
                     start_time, std::move(codec_created_cb)));
}

void CodecAllocator::ReleaseMediaCodec(std::unique_ptr<MediaCodecBridge> codec,
                                       base::OnceClosure codec_released_cb) {
  DCHECK(codec);
  DCHECK(codec_released_cb);

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CodecAllocator::ReleaseMediaCodec,
                       base::Unretained(this), std::move(codec),
                       BindToCurrentLoop(std::move(codec_released_cb))));
    return;
  }

  // Update |force_sw_codecs_| status.
  auto* task_runner = SelectCodecTaskRunner();

  // We always return non-software codecs to the primary task runner regardless
  // of whether it's hung or not. We don't want any non-sw codecs to hang the
  // the secondary task runner upon release.
  //
  // It's okay to release a software codec back to the primary task runner, we
  // just don't want to release non-sw codecs to the secondary task runner.
  if (codec->GetCodecType() != CodecType::kSoftware)
    task_runner = primary_task_runner_.get();

  const auto start_time = tick_clock_->NowTicks();
  pending_operations_.push_back(start_time);

  task_runner->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&ReleaseMediaCodecInternal, std::move(codec)),
      base::BindOnce(&CodecAllocator::OnCodecReleased, base::Unretained(this),
                     start_time, std::move(codec_released_cb)));
}

CodecAllocator::CodecAllocator(
    CodecFactoryCB factory_cb,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      factory_cb_(std::move(factory_cb)),
      tick_clock_(base::DefaultTickClock::GetInstance()) {}

CodecAllocator::~CodecAllocator() = default;

void CodecAllocator::OnCodecCreated(base::TimeTicks start_time,
                                    CodecCreatedCB codec_created_cb,
                                    std::unique_ptr<MediaCodecBridge> codec) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CompletePendingOperation(start_time);
  std::move(codec_created_cb).Run(std::move(codec));
}

void CodecAllocator::OnCodecReleased(base::TimeTicks start_time,
                                     base::OnceClosure codec_released_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CompletePendingOperation(start_time);
  std::move(codec_released_cb).Run();
}

bool CodecAllocator::IsPrimaryTaskRunnerLikelyHung() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Give tasks 800ms before considering them hung. MediaCodec.configure() calls
  // typically take 100-200ms on a N5, so 800ms is expected to very rarely
  // result in false positives. Also, false positives have low impact because we
  // resume using the thread when the task completes.
  constexpr base::TimeDelta kHungTaskDetectionTimeout =
      base::TimeDelta::FromMilliseconds(800);

  return !pending_operations_.empty() &&
         tick_clock_->NowTicks() - *pending_operations_.begin() >
             kHungTaskDetectionTimeout;
}

base::SequencedTaskRunner* CodecAllocator::SelectCodecTaskRunner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (IsPrimaryTaskRunnerLikelyHung()) {
    force_sw_codecs_ = true;
    if (!secondary_task_runner_)
      secondary_task_runner_ = CreateCodecTaskRunner();
    return secondary_task_runner_.get();
  }

  if (!primary_task_runner_)
    primary_task_runner_ = CreateCodecTaskRunner();

  force_sw_codecs_ = false;
  return primary_task_runner_.get();
}

void CodecAllocator::CompletePendingOperation(base::TimeTicks start_time) {
  // Note: This intentionally only erases the first instance, since there may be
  // multiple instances of the same value.
  pending_operations_.erase(std::find(pending_operations_.begin(),
                                      pending_operations_.end(), start_time));
}

}  // namespace media
