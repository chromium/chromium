// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/queue_report_time_swap_promise.h"

#if defined(OS_ANDROID)
#include "third_party/blink/public/platform/platform.h"
#endif

namespace blink {

QueueReportTimeSwapPromise::QueueReportTimeSwapPromise(
    int source_frame_number,
    DrainCallback drain_callback,
    base::OnceClosure swap_callback,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
    : source_frame_number_(source_frame_number),
      drain_callback_(std::move(drain_callback)),
      swap_callback_(std::move(swap_callback)),
#if defined(OS_ANDROID)
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
        FROM_HERE,
        base::BindOnce([](DrainCallback, base::OnceClosure) {},
                       std::move(drain_callback_), std::move(swap_callback_)));
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
    DidNotSwapReason reason) {
  if (reason == cc::SwapPromise::SWAP_FAILS ||
      reason == cc::SwapPromise::COMMIT_NO_UPDATE) {
    if (drain_callback_)
      std::move(drain_callback_).Run(source_frame_number_);
    if (swap_callback_)
      std::move(swap_callback_).Run();
  }
  return DidNotSwapAction::BREAK_PROMISE;
}

void QueueReportTimeSwapPromise::DidActivate() {
  if (drain_callback_)
    std::move(drain_callback_).Run(source_frame_number_);
#if defined(OS_ANDROID)
  if (call_swap_on_activate_ && swap_callback_)
    std::move(swap_callback_).Run();
#endif
}

}  // namespace blink
