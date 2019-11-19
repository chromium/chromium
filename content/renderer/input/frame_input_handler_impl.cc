// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/frame_input_handler_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "content/common/input/ime_text_span_conversions.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/widget_input_handler_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

FrameInputHandlerImpl::FrameInputHandlerImpl(
    base::WeakPtr<RenderFrameImpl> render_frame,
    mojo::PendingReceiver<mojom::FrameInputHandler> receiver)
    : render_frame_(render_frame),
      input_event_queue_(
          render_frame->GetLocalRootRenderWidget()->GetInputEventQueue()),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
  // If we have created an input event queue move the mojo request over to the
  // compositor thread.
  if (RenderThreadImpl::current() &&
      RenderThreadImpl::current()->compositor_task_runner() &&
      input_event_queue_) {
    // Mojo channel bound on compositor thread.
    RenderThreadImpl::current()->compositor_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FrameInputHandlerImpl::BindNow,
                                  base::Unretained(this), std::move(receiver)));
  } else {
    // Mojo channel bound on main thread.
    BindNow(std::move(receiver));
  }
}

FrameInputHandlerImpl::~FrameInputHandlerImpl() {}

// static
void FrameInputHandlerImpl::CreateMojoService(
    base::WeakPtr<RenderFrameImpl> render_frame,
    mojo::PendingReceiver<mojom::FrameInputHandler> receiver) {
  DCHECK(render_frame);

  // Owns itself. Will be deleted when message pipe is destroyed.
  new FrameInputHandlerImpl(render_frame, std::move(receiver));
}

void FrameInputHandlerImpl::RunOnMainThread(base::OnceClosure closure) {
  if (input_event_queue_) {
    input_event_queue_->QueueClosure(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void FrameInputHandlerImpl::SetCompositionFromExistingText(
    int32_t start,
    int32_t end,
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::BindOnce(&FrameInputHandlerImpl::SetCompositionFromExistingText,
                       weak_this_, start, end, ui_ime_text_spans));
    return;
  }

  if (!render_frame_)
    return;

  ImeEventGuard guard(render_frame_->GetLocalRootRenderWidget());

  render_frame_->GetWebFrame()->SetCompositionFromExistingText(
      start, end, ConvertUiImeTextSpansToBlinkImeTextSpans(ui_ime_text_spans));
}

void FrameInputHandlerImpl::ExtendSelectionAndDelete(int32_t before,
                                                     int32_t after) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::BindOnce(&FrameInputHandlerImpl::ExtendSelectionAndDelete,
                       weak_this_, before, after));
    return;
  }
  if (!render_frame_)
    return;
  render_frame_->GetWebFrame()->ExtendSelectionAndDelete(before, after);
}

void FrameInputHandlerImpl::DeleteSurroundingText(int32_t before,
                                                  int32_t after) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::BindOnce(&FrameInputHandlerImpl::DeleteSurroundingText,
                       weak_this_, before, after));
    return;
  }
  if (!render_frame_)
    return;
  render_frame_->GetWebFrame()->DeleteSurroundingText(before, after);
}

void FrameInputHandlerImpl::DeleteSurroundingTextInCodePoints(int32_t before,
                                                              int32_t after) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(base::BindOnce(
        &FrameInputHandlerImpl::DeleteSurroundingTextInCodePoints, weak_this_,
        before, after));
    return;
  }
  if (!render_frame_)
    return;
  render_frame_->GetWebFrame()->DeleteSurroundingTextInCodePoints(before,
                                                                  after);
}

void FrameInputHandlerImpl::SetEditableSelectionOffsets(int32_t start,
                                                        int32_t end) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::BindOnce(&FrameInputHandlerImpl::SetEditableSelectionOffsets,
                       weak_this_, start, end));
    return;
  }
  if (!render_frame_)
    return;
  HandlingState handling_state(render_frame_, UpdateState::kIsSelectingRange);
  render_frame_->GetWebFrame()->SetEditableSelectionOffsets(start, end);
}

void FrameInputHandlerImpl::ExecuteEditCommand(
    const std::string& command,
    const base::Optional<base::string16>& value) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(base::BindOnce(&FrameInputHandlerImpl::ExecuteEditCommand,
                                   weak_this_, command, value));
    return;
  }
  if (!render_frame_)
    return;
  if (value) {
    render_frame_->GetWebFrame()->ExecuteCommand(
        blink::WebString::FromUTF8(command),
        blink::WebString::FromUTF16(value.value()));
    return;
  }

  render_frame_->GetWebFrame()->ExecuteCommand(
      blink::WebString::FromUTF8(command));
}

void FrameInputHandlerImpl::Undo() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     weak_this_, "Undo", UpdateState::kNone));
}

void FrameInputHandlerImpl::Redo() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     weak_this_, "Redo", UpdateState::kNone));
}

void FrameInputHandlerImpl::Cut() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     weak_this_, "Cut", UpdateState::kIsSelectingRange));
}

void FrameInputHandlerImpl::Copy() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     weak_this_, "Copy", UpdateState::kIsSelectingRange));
}

void FrameInputHandlerImpl::CopyToFindPboard() {
#if defined(OS_MACOSX)
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::BindOnce(&FrameInputHandlerImpl::CopyToFindPboard, weak_this_));
    return;
  }
  if (!render_frame_)
    return;
  render_frame_->OnCopyToFindPboard();
#endif
}

void FrameInputHandlerImpl::Paste() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     weak_this_, "Paste", UpdateState::kIsPasting));
}

void FrameInputHandlerImpl::PasteAndMatchStyle() {
  RunOnMainThread(base::BindOnce(
      &FrameInputHandlerImpl::ExecuteCommandOnMainThread, weak_this_,
      "PasteAndMatchStyle", UpdateState::kIsPasting));
}

void FrameInputHandlerImpl::Replace(const base::string16& word) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::BindOnce(&FrameInputHandlerImpl::Replace, weak_this_, word));
    return;
  }
  if (!render_frame_)
    return;
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  if (!frame->HasSelection())
    frame->SelectWordAroundCaret();
  frame->ReplaceSelection(blink::WebString::FromUTF16(word));
  render_frame_->SyncSelectionIfRequired();
}

void FrameInputHandlerImpl::ReplaceMisspelling(const base::string16& word) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(base::BindOnce(&FrameInputHandlerImpl::ReplaceMisspelling,
                                   weak_this_, word));
    return;
  }
  if (!render_frame_)
    return;
  blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
  if (!frame->HasSelection())
    return;
  frame->ReplaceMisspelledRange(blink::WebString::FromUTF16(word));
}

void FrameInputHandlerImpl::Delete() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     weak_this_, "Delete", UpdateState::kNone));
}

void FrameInputHandlerImpl::SelectAll() {
  RunOnMainThread(
      base::BindOnce(&FrameInputHandlerImpl::ExecuteCommandOnMainThread,
                     weak_this_, "SelectAll", UpdateState::kIsSelectingRange));
}

void FrameInputHandlerImpl::CollapseSelection() {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::BindOnce(&FrameInputHandlerImpl::CollapseSelection, weak_this_));
    return;
  }

  if (!render_frame_)
    return;
  const blink::WebRange& range = render_frame_->GetWebFrame()
                                     ->GetInputMethodController()
                                     ->GetSelectionOffsets();
  if (range.IsNull())
    return;

  HandlingState handling_state(render_frame_, UpdateState::kIsSelectingRange);
  render_frame_->GetWebFrame()->SelectRange(
      blink::WebRange(range.EndOffset(), 0),
      blink::WebLocalFrame::kHideSelectionHandle,
      blink::mojom::SelectionMenuBehavior::kHide);
}

void FrameInputHandlerImpl::SelectRange(const gfx::Point& base,
                                        const gfx::Point& extent) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    // TODO(dtapuska): This event should be coalesced. Chrome IPC uses
    // one outstanding event and an ACK to handle coalescing on the browser
    // side. We should be able to clobber them in the main thread event queue.
    RunOnMainThread(base::BindOnce(&FrameInputHandlerImpl::SelectRange,
                                   weak_this_, base, extent));
    return;
  }

  if (!render_frame_)
    return;
  RenderWidget* window_widget = render_frame_->GetLocalRootRenderWidget();
  HandlingState handling_state(render_frame_, UpdateState::kIsSelectingRange);
  render_frame_->GetWebFrame()->SelectRange(
      window_widget->ConvertWindowPointToViewport(base),
      window_widget->ConvertWindowPointToViewport(extent));
}

#if defined(OS_ANDROID)
void FrameInputHandlerImpl::SelectWordAroundCaret(
    SelectWordAroundCaretCallback callback) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::BindOnce(&FrameInputHandlerImpl::SelectWordAroundCaret,
                       weak_this_, std::move(callback)));
    return;
  }

  bool did_select = false;
  int start_adjust = 0;
  int end_adjust = 0;
  if (render_frame_) {
    blink::WebLocalFrame* frame = render_frame_->GetWebFrame();
    blink::WebRange initial_range = frame->SelectionRange();
    render_frame_->GetLocalRootRenderWidget()->SetHandlingInputEvent(true);
    if (!initial_range.IsNull())
      did_select = frame->SelectWordAroundCaret();
    if (did_select) {
      blink::WebRange adjusted_range = frame->SelectionRange();
      DCHECK(!adjusted_range.IsNull());
      start_adjust = adjusted_range.StartOffset() - initial_range.StartOffset();
      end_adjust = adjusted_range.EndOffset() - initial_range.EndOffset();
    }
    render_frame_->GetLocalRootRenderWidget()->SetHandlingInputEvent(false);
  }

  // If the mojom channel is registered with compositor thread, we have to run
  // the callback on compositor thread. Otherwise run it on main thread. Mojom
  // requires the callback runs on the same thread.
  if (RenderThreadImpl::current() &&
      RenderThreadImpl::current()->compositor_task_runner() &&
      input_event_queue_) {
    RenderThreadImpl::current()->compositor_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), did_select, start_adjust,
                                  end_adjust));
  } else {
    std::move(callback).Run(did_select, start_adjust, end_adjust);
  }
}
#endif  // defined(OS_ANDROID)

void FrameInputHandlerImpl::AdjustSelectionByCharacterOffset(
    int32_t start,
    int32_t end,
    blink::mojom::SelectionMenuBehavior selection_menu_behavior) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::BindOnce(&FrameInputHandlerImpl::AdjustSelectionByCharacterOffset,
                       weak_this_, start, end, selection_menu_behavior));
    return;
  }

  if (!render_frame_)
    return;
  blink::WebRange range = render_frame_->GetWebFrame()
                              ->GetInputMethodController()
                              ->GetSelectionOffsets();
  if (range.IsNull())
    return;

  // Sanity checks to disallow empty and out of range selections.
  if (start - end > range.length() || range.StartOffset() + start < 0)
    return;

  HandlingState handling_state(render_frame_, UpdateState::kIsSelectingRange);
  // A negative adjust amount moves the selection towards the beginning of
  // the document, a positive amount moves the selection towards the end of
  // the document.
  render_frame_->GetWebFrame()->SelectRange(
      blink::WebRange(range.StartOffset() + start,
                      range.length() + end - start),
      blink::WebLocalFrame::kPreserveHandleVisibility, selection_menu_behavior);
}

void FrameInputHandlerImpl::MoveRangeSelectionExtent(const gfx::Point& extent) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    // TODO(dtapuska): This event should be coalesced. Chrome IPC uses
    // one outstanding event and an ACK to handle coalescing on the browser
    // side. We should be able to clobber them in the main thread event queue.
    RunOnMainThread(base::BindOnce(
        &FrameInputHandlerImpl::MoveRangeSelectionExtent, weak_this_, extent));
    return;
  }

  if (!render_frame_)
    return;
  HandlingState handling_state(render_frame_, UpdateState::kIsSelectingRange);
  render_frame_->GetWebFrame()->MoveRangeSelectionExtent(
      render_frame_->GetLocalRootRenderWidget()->ConvertWindowPointToViewport(
          extent));
}

void FrameInputHandlerImpl::ScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(base::BindOnce(
        &FrameInputHandlerImpl::ScrollFocusedEditableNodeIntoRect, weak_this_,
        rect));
    return;
  }

  if (!render_frame_)
    return;

  // OnSynchronizeVisualProperties does not call DidChangeVisibleViewport
  // on OOPIFs. Since we are starting a new scroll operation now, call
  // DidChangeVisibleViewport to ensure that we don't assume the element
  // is already in view and ignore the scroll.
  render_frame_->ResetHasScrolledFocusedEditableIntoView();
  render_frame_->ScrollFocusedEditableElementIntoRect(rect);
}

void FrameInputHandlerImpl::MoveCaret(const gfx::Point& point) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    RunOnMainThread(
        base::BindOnce(&FrameInputHandlerImpl::MoveCaret, weak_this_, point));
    return;
  }

  if (!render_frame_)
    return;

  render_frame_->GetWebFrame()->MoveCaretSelection(
      render_frame_->GetLocalRootRenderWidget()->ConvertWindowPointToViewport(
          point));
}

void FrameInputHandlerImpl::GetWidgetInputHandler(
    mojo::PendingAssociatedReceiver<mojom::WidgetInputHandler> receiver,
    mojo::PendingRemote<mojom::WidgetInputHandlerHost> host) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FrameInputHandlerImpl::GetWidgetInputHandler,
                       weak_this_, std::move(receiver), std::move(host)));
    return;
  }
  if (!render_frame_)
    return;
  render_frame_->GetLocalRootRenderWidget()
      ->widget_input_handler_manager()
      ->AddAssociatedInterface(std::move(receiver), std::move(host));
}

void FrameInputHandlerImpl::ExecuteCommandOnMainThread(
    const std::string& command,
    UpdateState update_state) {
  if (!render_frame_)
    return;

  HandlingState handling_state(render_frame_, update_state);
  render_frame_->GetWebFrame()->ExecuteCommand(
      blink::WebString::FromUTF8(command));
}

void FrameInputHandlerImpl::Release() {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    // Close the receiver on the compositor thread first before telling the main
    // thread to delete this object.
    receiver_.reset();
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FrameInputHandlerImpl::Release, weak_this_));
    return;
  }
  delete this;
}

void FrameInputHandlerImpl::BindNow(
    mojo::PendingReceiver<mojom::FrameInputHandler> receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&FrameInputHandlerImpl::Release, base::Unretained(this)));
}

FrameInputHandlerImpl::HandlingState::HandlingState(
    const base::WeakPtr<RenderFrameImpl>& render_frame,
    UpdateState state)
    : render_frame_(render_frame),
      original_select_range_value_(render_frame->handling_select_range()),
      original_pasting_value_(render_frame->IsPasting()) {
  switch (state) {
    case UpdateState::kIsPasting:
      render_frame->set_is_pasting(true);
      FALLTHROUGH;  // Matches RenderFrameImpl::OnPaste() which sets both.
    case UpdateState::kIsSelectingRange:
      render_frame->set_handling_select_range(true);
      break;
    case UpdateState::kNone:
      break;
  }
}

FrameInputHandlerImpl::HandlingState::~HandlingState() {
  // RenderFrame may have been destroyed while this object was on the stack.
  if (!render_frame_)
    return;
  render_frame_->set_handling_select_range(original_select_range_value_);
  render_frame_->set_is_pasting(original_pasting_value_);
}

}  // namespace content
