// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_impl.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/input/frame_widget_input_handler_impl.h"
#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {

namespace {

void RunClosureIfNotSwappedOut(base::WeakPtr<WidgetBase> widget,
                               base::OnceClosure closure) {
  // Input messages must not be processed if the WidgetBase was destroyed or
  // was just recreated for a provisional frame.
  if (!widget || widget->IsForProvisionalFrame()) {
    return;
  }
  std::move(closure).Run();
}

}  // namespace

WidgetInputHandlerImpl::WidgetInputHandlerImpl(
    scoped_refptr<WidgetInputHandlerManager> manager,
    scoped_refptr<MainThreadEventQueue> input_event_queue,
    base::WeakPtr<WidgetBase> widget,
    base::WeakPtr<mojom::blink::FrameWidgetInputHandler>
        frame_widget_input_handler)
    : input_handler_manager_(manager),
      input_event_queue_(input_event_queue),
      widget_(std::move(widget)),
      frame_widget_input_handler_(std::move(frame_widget_input_handler)) {
  // NOTE: DirectReceiver must be bound on an IO thread, so input handlers which
  // live on the main thread (e.g. for popups) cannot use direct IPC for now.
  if (base::FeatureList::IsEnabled(features::kDirectCompositorThreadIpc) &&
      base::CurrentIOThread::IsSet() && mojo::IsDirectReceiverSupported()) {
    receiver_.emplace<DirectReceiver>(mojo::DirectReceiverKey{}, this);
  } else {
    receiver_.emplace<Receiver>(this);
  }
}

WidgetInputHandlerImpl::~WidgetInputHandlerImpl() = default;

void WidgetInputHandlerImpl::SetReceiver(
    mojo::PendingReceiver<mojom::blink::WidgetInputHandler>
        interface_receiver) {
  if (absl::holds_alternative<Receiver>(receiver_)) {
    auto& receiver = absl::get<Receiver>(receiver_);
    receiver.Bind(std::move(interface_receiver));
    receiver.set_disconnect_handler(base::BindOnce(
        &WidgetInputHandlerImpl::Release, base::Unretained(this)));
  } else {
    CHECK(absl::holds_alternative<DirectReceiver>(receiver_));
    auto& receiver = absl::get<DirectReceiver>(receiver_);
    receiver.Bind(std::move(interface_receiver));
    receiver.set_disconnect_handler(base::BindOnce(
        &WidgetInputHandlerImpl::Release, base::Unretained(this)));
  }
}

void WidgetInputHandlerImpl::SetFocus(mojom::blink::FocusState focus_state) {
  RunOnMainThread(base::BindOnce(&WidgetBase::SetFocus, widget_, focus_state));
}

void WidgetInputHandlerImpl::MouseCaptureLost() {
  RunOnMainThread(base::BindOnce(&WidgetBase::MouseCaptureLost, widget_));
}

void WidgetInputHandlerImpl::SetEditCommandsForNextKeyEvent(
    Vector<mojom::blink::EditCommandPtr> commands) {
  RunOnMainThread(base::BindOnce(&WidgetBase::SetEditCommandsForNextKeyEvent,
                                 widget_, std::move(commands)));
}

void WidgetInputHandlerImpl::CursorVisibilityChanged(bool visible) {
  RunOnMainThread(
      base::BindOnce(&WidgetBase::CursorVisibilityChange, widget_, visible));
}

static void ImeSetCompositionOnMainThread(
    base::WeakPtr<WidgetBase> widget,
    scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
    const String& text,
    const Vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& range,
    int32_t start,
    int32_t end,
    WidgetInputHandlerImpl::ImeSetCompositionCallback callback) {
  widget->ImeSetComposition(text, ime_text_spans, range, start, end);
  callback_task_runner->PostTask(FROM_HERE, std::move(callback));
}

void WidgetInputHandlerImpl::ImeSetComposition(
    const String& text,
    const Vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& range,
    int32_t start,
    int32_t end,
    WidgetInputHandlerImpl::ImeSetCompositionCallback callback) {
  RunOnMainThread(
      base::BindOnce(&ImeSetCompositionOnMainThread, widget_,
                     base::SingleThreadTaskRunner::GetCurrentDefault(), text,
                     ime_text_spans, range, start, end, std::move(callback)));
}

static void ImeCommitTextOnMainThread(
    base::WeakPtr<WidgetBase> widget,
    scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
    const String& text,
    const Vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& range,
    int32_t relative_cursor_position,
    WidgetInputHandlerImpl::ImeCommitTextCallback callback) {
  widget->ImeCommitText(text, ime_text_spans, range, relative_cursor_position);
  callback_task_runner->PostTask(FROM_HERE, std::move(callback));
}

void WidgetInputHandlerImpl::ImeCommitText(
    const String& text,
    const Vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& range,
    int32_t relative_cursor_position,
    ImeCommitTextCallback callback) {
  RunOnMainThread(base::BindOnce(
      &ImeCommitTextOnMainThread, widget_,
      base::SingleThreadTaskRunner::GetCurrentDefault(), text, ime_text_spans,
      range, relative_cursor_position, std::move(callback)));
}

void WidgetInputHandlerImpl::ImeFinishComposingText(bool keep_selection) {
  RunOnMainThread(base::BindOnce(&WidgetBase::ImeFinishComposingText, widget_,
                                 keep_selection));
}

void WidgetInputHandlerImpl::RequestTextInputStateUpdate() {
  RunOnMainThread(
      base::BindOnce(&WidgetBase::ForceTextInputStateUpdate, widget_));
}

void WidgetInputHandlerImpl::RequestCompositionUpdates(bool immediate_request,
                                                       bool monitor_request) {
  RunOnMainThread(base::BindOnce(&WidgetBase::RequestCompositionUpdates,
                                 widget_, immediate_request, monitor_request));
}

void WidgetInputHandlerImpl::DispatchEvent(
    std::unique_ptr<WebCoalescedInputEvent> event,
    DispatchEventCallback callback) {
  TRACE_EVENT0("input,input.scrolling",
               "WidgetInputHandlerImpl::DispatchEvent");
  input_handler_manager_->DispatchEvent(std::move(event), std::move(callback));
}

void WidgetInputHandlerImpl::DispatchNonBlockingEvent(
    std::unique_ptr<WebCoalescedInputEvent> event) {
  TRACE_EVENT0("input,input.scrolling",
               "WidgetInputHandlerImpl::DispatchNonBlockingEvent");
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

#if BUILDFLAG(IS_ANDROID)
void WidgetInputHandlerImpl::AttachSynchronousCompositor(
    mojo::PendingRemote<mojom::blink::SynchronousCompositorControlHost>
        control_host,
    mojo::PendingAssociatedRemote<mojom::blink::SynchronousCompositorHost> host,
    mojo::PendingAssociatedReceiver<mojom::blink::SynchronousCompositor>
        compositor_receiver) {
  input_handler_manager_->AttachSynchronousCompositor(
      std::move(control_host), std::move(host), std::move(compositor_receiver));
}
#endif

void WidgetInputHandlerImpl::GetFrameWidgetInputHandler(
    mojo::PendingAssociatedReceiver<mojom::blink::FrameWidgetInputHandler>
        frame_receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<FrameWidgetInputHandlerImpl>(
          widget_, frame_widget_input_handler_, input_event_queue_),
      std::move(frame_receiver));
}

void WidgetInputHandlerImpl::UpdateBrowserControlsState(
    cc::BrowserControlsState constraints,
    cc::BrowserControlsState current,
    bool animate,
    const std::optional<cc::BrowserControlsOffsetTagsInfo>& offset_tags_info) {
  input_handler_manager_->UpdateBrowserControlsState(constraints, current,
                                                     animate, offset_tags_info);
}

void WidgetInputHandlerImpl::RunOnMainThread(base::OnceClosure closure) {
  if (ThreadedCompositingEnabled()) {
    input_event_queue_->QueueClosure(base::BindOnce(
        &RunClosureIfNotSwappedOut, widget_, std::move(closure)));
  } else {
    RunClosureIfNotSwappedOut(widget_, std::move(closure));
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

  delete this;
}

}  // namespace blink
