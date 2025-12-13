// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/widget_compositor.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/renderer/platform/widget/compositing/queue_report_time_swap_promise.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"

namespace blink {

// static
scoped_refptr<WidgetCompositor> WidgetCompositor::Create(
    base::WeakPtr<WidgetBase> widget_base,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    mojo::PendingReceiver<mojom::blink::WidgetCompositor> receiver) {
  auto compositor = base::MakeRefCounted<WidgetCompositor>(
      WidgetCompositorPassKeyProvider::GetPassKey(), std::move(widget_base),
      std::move(main_task_runner), std::move(compositor_task_runner));
  compositor->BindOnThread(std::move(receiver));
  return compositor;
}

WidgetCompositor::WidgetCompositor(
    base::PassKey<WidgetCompositorPassKeyProvider>,
    base::WeakPtr<WidgetBase> widget_base,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
    : widget_base_(std::move(widget_base)),
      main_task_runner_(std::move(main_task_runner)),
      compositor_task_runner_(std::move(compositor_task_runner)),
      swap_queue_(std::make_unique<WidgetSwapQueue>()) {}

void WidgetCompositor::Shutdown() {
  if (!compositor_task_runner_) {
    ResetOnThread();
  } else {
    compositor_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WidgetCompositor::ResetOnThread,
                                  scoped_refptr<WidgetCompositor>(this)));
  }
}

void WidgetCompositor::BindOnThread(
    mojo::PendingReceiver<mojom::blink::WidgetCompositor> receiver) {
  if (CalledOnValidCompositorThread()) {
    receiver_.Bind(std::move(receiver), compositor_task_runner_);
  } else {
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WidgetCompositor::BindOnThread, base::RetainedRef(this),
                       std::move(receiver)));
  }
}

void WidgetCompositor::ResetOnThread() {
  DCHECK(CalledOnValidCompositorThread());
  receiver_.reset();
}

void WidgetCompositor::VisualStateRequest(VisualStateRequestCallback callback) {
  DCHECK(CalledOnValidCompositorThread());

  auto drain_callback =
      base::BindOnce(&WidgetCompositor::DrainQueue, base::RetainedRef(this));
  auto swap_callback = base::BindOnce(&WidgetCompositor::VisualStateResponse,
                                      base::RetainedRef(this));
  if (!compositor_task_runner_) {
    CreateQueueSwapPromise(std::move(drain_callback), std::move(swap_callback),
                           std::move(callback));
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WidgetCompositor::CreateQueueSwapPromise,
                       base::RetainedRef(this), std::move(drain_callback),
                       std::move(swap_callback), std::move(callback)));
  }
}

cc::LayerTreeHost* WidgetCompositor::LayerTreeHost() const {
  return widget_base_->LayerTreeHost();
}

void WidgetCompositor::CreateQueueSwapPromise(
    base::OnceCallback<void(int)> drain_callback,
    base::OnceClosure swap_callback,
    VisualStateRequestCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  bool first_message_for_frame = false;
  int source_frame_number = 0;
  if (widget_base_) {
    source_frame_number = LayerTreeHost()->SourceFrameNumber();
    swap_queue_->Queue(source_frame_number, std::move(callback),
                       &first_message_for_frame);
  }

  if (first_message_for_frame) {
    LayerTreeHost()->QueueSwapPromise(
        std::make_unique<QueueReportTimeSwapPromise>(
            source_frame_number, std::move(drain_callback),
            std::move(swap_callback), compositor_task_runner_));
    // Request a main frame if one is not already in progress. This might either
    // A) request a commit ahead of time or B) request a commit which is not
    // needed because there are not pending updates. If B) then the frame will
    // be aborted early and the swap promises will be broken (see
    // EarlyOut_NoUpdates).
    LayerTreeHost()->SetNeedsAnimateIfNotInsideMainFrame();

    // In web tests the request does not actually cause a commit, because the
    // compositor is scheduled by the test runner to avoid flakiness. So for
    // this case we must request a main frame.
    widget_base_->client()->ScheduleAnimationForWebTests();
  } else if (compositor_task_runner_) {
    // Delete callbacks on the compositor thread.
    compositor_task_runner_->PostTask(
        FROM_HERE, base::DoNothingWithBoundArgs(std::move(drain_callback),
                                                std::move(swap_callback)));
  }
}

void WidgetCompositor::VisualStateResponse() {
  DCHECK(CalledOnValidCompositorThread());
  Vector<VisualStateRequestCallback> callbacks;
  swap_queue_->GetCallbacks(&callbacks);
  for (auto& callback : callbacks)
    std::move(callback).Run();
}

void WidgetCompositor::DrainQueue(int source_frame_number) {
  DCHECK(CalledOnValidCompositorThread());
  swap_queue_->Drain(source_frame_number);
}

bool WidgetCompositor::CalledOnValidCompositorThread() {
  return !compositor_task_runner_ ||
         compositor_task_runner_->BelongsToCurrentThread();
}

}  // namespace blink
