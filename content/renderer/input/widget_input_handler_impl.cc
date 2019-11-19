// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/widget_input_handler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "content/common/input/ime_text_span_conversions.h"
#include "content/common/input_messages.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/widget_input_handler_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

namespace {

void RunClosureIfNotSwappedOut(base::WeakPtr<RenderWidget> render_widget,
                               base::OnceClosure closure) {
  // Input messages must not be processed if the RenderWidget was undead or is
  // closing.
  if (!render_widget || render_widget->IsUndeadOrProvisional()) {
    return;
  }
  std::move(closure).Run();
}

}  // namespace

WidgetInputHandlerImpl::WidgetInputHandlerImpl(
    scoped_refptr<WidgetInputHandlerManager> manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    scoped_refptr<MainThreadEventQueue> input_event_queue,
    base::WeakPtr<RenderWidget> render_widget)
    : main_thread_task_runner_(main_thread_task_runner),
      input_handler_manager_(manager),
      input_event_queue_(input_event_queue),
      render_widget_(render_widget) {}

WidgetInputHandlerImpl::~WidgetInputHandlerImpl() {}

void WidgetInputHandlerImpl::SetAssociatedReceiver(
    mojo::PendingAssociatedReceiver<mojom::WidgetInputHandler> receiver) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  if (content::RenderThreadImpl::current()) {
    blink::scheduler::WebThreadScheduler* scheduler =
        content::RenderThreadImpl::current()->GetWebMainThreadScheduler();
    task_runner = scheduler->DeprecatedDefaultTaskRunner();
  }
  associated_receiver_.Bind(std::move(receiver), std::move(task_runner));
  associated_receiver_.set_disconnect_handler(
      base::BindOnce(&WidgetInputHandlerImpl::Release, base::Unretained(this)));
}

void WidgetInputHandlerImpl::SetReceiver(
    mojo::PendingReceiver<mojom::WidgetInputHandler> interface_receiver) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  if (content::RenderThreadImpl::current()) {
    blink::scheduler::WebThreadScheduler* scheduler =
        content::RenderThreadImpl::current()->GetWebMainThreadScheduler();
    task_runner = scheduler->DeprecatedDefaultTaskRunner();
  }
  receiver_.Bind(std::move(interface_receiver), std::move(task_runner));
  receiver_.set_disconnect_handler(
      base::BindOnce(&WidgetInputHandlerImpl::Release, base::Unretained(this)));
}

void WidgetInputHandlerImpl::SetFocus(bool focused) {
  RunOnMainThread(
      base::BindOnce(&RenderWidget::OnSetFocus, render_widget_, focused));
}

void WidgetInputHandlerImpl::MouseCaptureLost() {
  RunOnMainThread(
      base::BindOnce(&RenderWidget::OnMouseCaptureLost, render_widget_));
}

void WidgetInputHandlerImpl::SetEditCommandsForNextKeyEvent(
    const std::vector<EditCommand>& commands) {
  RunOnMainThread(
      base::BindOnce(&RenderWidget::OnSetEditCommandsForNextKeyEvent,
                     render_widget_, commands));
}

void WidgetInputHandlerImpl::CursorVisibilityChanged(bool visible) {
  RunOnMainThread(base::BindOnce(&RenderWidget::OnCursorVisibilityChange,
                                 render_widget_, visible));
}

void WidgetInputHandlerImpl::FallbackCursorModeToggled(bool is_on) {
  RunOnMainThread(base::BindOnce(&RenderWidget::OnFallbackCursorModeToggled,
                                 render_widget_, is_on));
}

void WidgetInputHandlerImpl::ImeSetComposition(
    const base::string16& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& range,
    int32_t start,
    int32_t end) {
  RunOnMainThread(
      base::BindOnce(&RenderWidget::OnImeSetComposition, render_widget_, text,
                     ConvertUiImeTextSpansToBlinkImeTextSpans(ime_text_spans),
                     range, start, end));
}

static void ImeCommitTextOnMainThread(
    base::WeakPtr<RenderWidget> render_widget,
    scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
    const base::string16& text,
    const std::vector<blink::WebImeTextSpan>& ime_text_spans,
    const gfx::Range& range,
    int32_t relative_cursor_position,
    WidgetInputHandlerImpl::ImeCommitTextCallback callback) {
  render_widget->OnImeCommitText(text, ime_text_spans, range,
                                 relative_cursor_position);
  callback_task_runner->PostTask(FROM_HERE, std::move(callback));
}

void WidgetInputHandlerImpl::ImeCommitText(
    const base::string16& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& range,
    int32_t relative_cursor_position,
    ImeCommitTextCallback callback) {
  RunOnMainThread(
      base::BindOnce(&ImeCommitTextOnMainThread, render_widget_,
                     base::ThreadTaskRunnerHandle::Get(), text,
                     ConvertUiImeTextSpansToBlinkImeTextSpans(ime_text_spans),
                     range, relative_cursor_position, std::move(callback)));
}

void WidgetInputHandlerImpl::ImeFinishComposingText(bool keep_selection) {
  RunOnMainThread(base::BindOnce(&RenderWidget::OnImeFinishComposingText,
                                 render_widget_, keep_selection));
}

void WidgetInputHandlerImpl::RequestTextInputStateUpdate() {
  RunOnMainThread(base::BindOnce(&RenderWidget::OnRequestTextInputStateUpdate,
                                 render_widget_));
}

void WidgetInputHandlerImpl::RequestCompositionUpdates(bool immediate_request,
                                                       bool monitor_request) {
  RunOnMainThread(base::BindOnce(&RenderWidget::OnRequestCompositionUpdates,
                                 render_widget_, immediate_request,
                                 monitor_request));
}

void WidgetInputHandlerImpl::DispatchEvent(
    std::unique_ptr<content::InputEvent> event,
    DispatchEventCallback callback) {
  TRACE_EVENT0("input", "WidgetInputHandlerImpl::DispatchEvent");
  input_handler_manager_->DispatchEvent(std::move(event), std::move(callback));
}

void WidgetInputHandlerImpl::DispatchNonBlockingEvent(
    std::unique_ptr<content::InputEvent> event) {
  TRACE_EVENT0("input", "WidgetInputHandlerImpl::DispatchNonBlockingEvent");
  input_handler_manager_->DispatchEvent(std::move(event),
                                        DispatchEventCallback());
}

void WidgetInputHandlerImpl::WaitForInputProcessed(
    WaitForInputProcessedCallback callback) {
  DCHECK(!input_processed_ack_);

  // Store so that we can respond even if the renderer is destructed.
  input_processed_ack_ = std::move(callback);

  input_handler_manager_->WaitForInputProcessed(
      base::BindOnce(&WidgetInputHandlerImpl::InputWasProcessed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WidgetInputHandlerImpl::InputWasProcessed() {
  // The callback can be be invoked when the renderer is hidden and then again
  // when it's shown. We can also be called after Release is called so always
  // check that the callback exists.
  if (input_processed_ack_)
    std::move(input_processed_ack_).Run();
}

void WidgetInputHandlerImpl::AttachSynchronousCompositor(
    mojo::PendingRemote<mojom::SynchronousCompositorControlHost> control_host,
    mojo::PendingAssociatedRemote<mojom::SynchronousCompositorHost> host,
    mojo::PendingAssociatedReceiver<mojom::SynchronousCompositor>
        compositor_receiver) {
  input_handler_manager_->AttachSynchronousCompositor(
      std::move(control_host), std::move(host), std::move(compositor_receiver));
}

void WidgetInputHandlerImpl::RunOnMainThread(base::OnceClosure closure) {
  if (input_event_queue_) {
    input_event_queue_->QueueClosure(base::BindOnce(
        &RunClosureIfNotSwappedOut, render_widget_, std::move(closure)));
  } else {
    RunClosureIfNotSwappedOut(render_widget_, std::move(closure));
  }
}

void WidgetInputHandlerImpl::Release() {
  // If the renderer is closed, make sure we ack the outstanding Mojo callback
  // so that we don't DCHECK and/or leave the browser-side blocked for an ACK
  // that will never come if the renderer is destroyed before this callback is
  // invoked. Note, this method will always be called on the Mojo-bound thread
  // first and then again on the main thread, the callback will always be
  // called on the Mojo-bound thread though.
  if (input_processed_ack_)
    std::move(input_processed_ack_).Run();

  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    // Close the binding on the compositor thread first before telling the main
    // thread to delete this object.
    associated_receiver_.reset();
    receiver_.reset();
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WidgetInputHandlerImpl::Release,
                                  base::Unretained(this)));
    return;
  }
  delete this;
}

}  // namespace content
