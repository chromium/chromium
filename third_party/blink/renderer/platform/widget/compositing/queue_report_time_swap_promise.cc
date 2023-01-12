// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/queue_report_time_swap_promise.h"

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/platform/platform.h"
#endif

namespace blink {
namespace {

void RunDrainAndSwapCallbacksOnCompositorThread(
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    QueueReportTimeSwapPromise::DrainCallback drain_callback,
    int source_frame_number,
    base::OnceClosure swap_callback) {
  if (compositor_task_runner &&
      !compositor_task_runner->BelongsToCurrentThread()) {
    compositor_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&RunDrainAndSwapCallbacksOnCompositorThread, nullptr,
                       std::move(drain_callback), source_frame_number,
                       std::move(swap_callback)));
    return;
  }

  if (drain_callback)
    std::move(drain_callback).Run(source_frame_number);
  if (swap_callback)
    std::move(swap_callback).Run();
}

}  // namespace

QueueReportTimeSwapPromise::QueueReportTimeSwapPromise(
    int source_frame_number,
    DrainCallback drain_callback,
    base::OnceClosure swap_callback,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
    : source_frame_number_(source_frame_number),
      drain_callback_(std::move(drain_callback)),
      swap_callback_(std::move(swap_callback)),
#if BUILDFLAG(IS_ANDROID)
      call_swap_on_activate_(
          Platform::Current()
              ->IsSynchronousCompositingEnabledForAndroidWebView()),
#endif
      compositor_task_runner_(std::move(compositor_task_runner)) {
}

QueueReportTimeSwapPromise::~QueueReportTimeSwapPromise() {
  if (compositor_task_runner_ && (drain_callback_ || swap_callback_)) {
    DCHECK(!compositor_task_runner_->BelongsToCurrentThread());
    compositor_task_runner_->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(std::move(drain_callback_),
                                                std::move(swap_callback_)));
  }
}

void QueueReportTimeSwapPromise::WillSwap(
    viz::CompositorFrameMetadata* metadata) {
  DCHECK_GT(metadata->frame_token, 0u);
}

void QueueReportTimeSwapPromise::DidSwap() {
  if (swap_callback_)
    std::move(swap_callback_).Run();
}

cc::SwapPromise::DidNotSwapAction QueueReportTimeSwapPromise::DidNotSwap(
    DidNotSwapReason reason,
    base::TimeTicks ts) {
  if (reason == cc::SwapPromise::COMMIT_FAILS)
    return DidNotSwapAction::KEEP_ACTIVE;

  if (reason == cc::SwapPromise::SWAP_FAILS ||
      reason == cc::SwapPromise::COMMIT_NO_UPDATE) {
    // Since `DidNotSwap()` can be called on any thread, run drain and swap
    // callbacks on the compositor thread if there is one.
    RunDrainAndSwapCallbacksOnCompositorThread(
        compositor_task_runner_, std::move(drain_callback_),
        source_frame_number_, std::move(swap_callback_));
  }
  return DidNotSwapAction::BREAK_PROMISE;
}

void QueueReportTimeSwapPromise::DidActivate() {
  if (drain_callback_)
    std::move(drain_callback_).Run(source_frame_number_);
#if BUILDFLAG(IS_ANDROID)
  if (call_swap_on_activate_ && swap_callback_)
    std::move(swap_callback_).Run();
#endif
}

}  // namespace blink
