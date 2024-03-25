// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/frame_widget_input_handler_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/input/handwriting_gesture_result.mojom-blink.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/input/ime_event_guard.h"
#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"

namespace blink {

FrameWidgetInputHandlerImpl::FrameWidgetInputHandlerImpl(
    base::WeakPtr<WidgetBase> widget,
    base::WeakPtr<mojom::blink::FrameWidgetInputHandler>
        frame_widget_input_handler,
    scoped_refptr<MainThreadEventQueue> input_event_queue)
    : widget_(std::move(widget)),
      main_thread_frame_widget_input_handler_(
          std::move(frame_widget_input_handler)),
      input_event_queue_(input_event_queue) {}

FrameWidgetInputHandlerImpl::~FrameWidgetInputHandlerImpl() = default;

void FrameWidgetInputHandlerImpl::RunOnMainThread(base::OnceClosure closure) {
  if (ThreadedCompositingEnabled()) {
    input_event_queue_->QueueClosure(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void FrameWidgetInputHandlerImpl::AddImeTextSpansToExistingText(
    uint32_t start,
    uint32_t end,
    const Vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget,
         base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         uint32_t start, uint32_t end,
         const Vector<ui::ImeTextSpan>& ui_ime_text_spans) {
        DCHECK_EQ(!!widget, !!handler);
        if (!widget)
          return;
        ImeEventGuard guard(widget);
        handler->AddImeTextSpansToExistingText(start, end, ui_ime_text_spans);
      },
      widget_, main_thread_frame_widget_input_handler_, start, end,
      ui_ime_text_spans));
}

void FrameWidgetInputHandlerImpl::ClearImeTextSpansByType(
    uint32_t start,
    uint32_t end,
    ui::ImeTextSpan::Type type) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget,
         base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         uint32_t start, uint32_t end, ui::ImeTextSpan::Type type) {
        DCHECK_EQ(!!widget, !!handler);
        if (!widget)
          return;
        ImeEventGuard guard(widget);
        handler->ClearImeTextSpansByType(start, end, type);
      },
      widget_, main_thread_frame_widget_input_handler_, start, end, type));
}

void FrameWidgetInputHandlerImpl::SetCompositionFromExistingText(
    int32_t start,
    int32_t end,
    const Vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget,
         base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         int32_t start, int32_t end,
         const Vector<ui::ImeTextSpan>& ui_ime_text_spans) {
        DCHECK_EQ(!!widget, !!handler);
        if (!widget)
          return;
        ImeEventGuard guard(widget);
        handler->SetCompositionFromExistingText(start, end, ui_ime_text_spans);
      },
      widget_, main_thread_frame_widget_input_handler_, start, end,
      ui_ime_text_spans));
}

void FrameWidgetInputHandlerImpl::ExtendSelectionAndDelete(int32_t before,
                                                           int32_t after) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         int32_t before, int32_t after) {
        if (handler)
          handler->ExtendSelectionAndDelete(before, after);
      },
      main_thread_frame_widget_input_handler_, before, after));
}

void FrameWidgetInputHandlerImpl::ExtendSelectionAndReplace(
    uint32_t before,
    uint32_t after,
    const String& replacement_text) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         uint32_t before, uint32_t after, const String& replacement_text) {
        if (handler) {
          handler->ExtendSelectionAndReplace(before, after, replacement_text);
        }
      },
      main_thread_frame_widget_input_handler_, before, after,
      replacement_text));
}

void FrameWidgetInputHandlerImpl::DeleteSurroundingText(int32_t before,
                                                        int32_t after) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         int32_t before, int32_t after) {
        if (handler)
          handler->DeleteSurroundingText(before, after);
      },
      main_thread_frame_widget_input_handler_, before, after));
}

void FrameWidgetInputHandlerImpl::DeleteSurroundingTextInCodePoints(
    int32_t before,
    int32_t after) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         int32_t before, int32_t after) {
        if (handler)
          handler->DeleteSurroundingTextInCodePoints(before, after);
      },
      main_thread_frame_widget_input_handler_, before, after));
}

void FrameWidgetInputHandlerImpl::SetEditableSelectionOffsets(int32_t start,
                                                              int32_t end) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget,
         base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         int32_t start, int32_t end) {
        DCHECK_EQ(!!widget, !!handler);
        if (!widget)
          return;
        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        handler->SetEditableSelectionOffsets(start, end);
      },
      widget_, main_thread_frame_widget_input_handler_, start, end));
}

void FrameWidgetInputHandlerImpl::HandleStylusWritingGestureAction(
    mojom::blink::StylusWritingGestureDataPtr gesture_data,
    HandleStylusWritingGestureActionCallback callback) {
  if (ThreadedCompositingEnabled()) {
    callback = base::BindOnce(
        [](scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
           HandleStylusWritingGestureActionCallback callback,
           mojom::blink::HandwritingGestureResult result) {
          callback_task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), std::move(result)));
        },
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(callback));
  }

  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         mojom::blink::StylusWritingGestureDataPtr gesture_data,
         HandleStylusWritingGestureActionCallback callback) {
        if (handler) {
          handler->HandleStylusWritingGestureAction(std::move(gesture_data),
                                                    std::move(callback));
        } else {
          std::move(callback).Run(
              mojom::blink::HandwritingGestureResult::kFailed);
        }
      },
      main_thread_frame_widget_input_handler_, std::move(gesture_data),
      std::move(callback)));
}

void FrameWidgetInputHandlerImpl::ExecuteEditCommand(const String& command,
                                                     const String& value) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         const String& command, const String& value) {
        if (handler)
          handler->ExecuteEditCommand(command, value);
      },
      main_thread_frame_widget_input_handler_, command, value));
}

void FrameWidgetInputHandlerImpl::Undo() {
  RunOnMainThread(base::BindOnce(
      &FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread, widget_,
      main_thread_frame_widget_input_handler_, "Undo", UpdateState::kNone));
}

void FrameWidgetInputHandlerImpl::Redo() {
  RunOnMainThread(base::BindOnce(
      &FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread, widget_,
      main_thread_frame_widget_input_handler_, "Redo", UpdateState::kNone));
}

void FrameWidgetInputHandlerImpl::Cut() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, main_thread_frame_widget_input_handler_, "Cut",
                     UpdateState::kIsSelectingRange));
}

void FrameWidgetInputHandlerImpl::Copy() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, main_thread_frame_widget_input_handler_, "Copy",
                     UpdateState::kIsSelectingRange));
}

void FrameWidgetInputHandlerImpl::CopyToFindPboard() {
#if BUILDFLAG(IS_MAC)
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler) {
        if (handler)
          handler->CopyToFindPboard();
      },
      main_thread_frame_widget_input_handler_));
#endif
}

void FrameWidgetInputHandlerImpl::CenterSelection() {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler) {
        if (handler) {
          handler->CenterSelection();
        }
      },
      main_thread_frame_widget_input_handler_));
}

void FrameWidgetInputHandlerImpl::Paste() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, main_thread_frame_widget_input_handler_, "Paste",
                     UpdateState::kIsPasting));
}

void FrameWidgetInputHandlerImpl::PasteAndMatchStyle() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, main_thread_frame_widget_input_handler_,
                     "PasteAndMatchStyle", UpdateState::kIsPasting));
}

void FrameWidgetInputHandlerImpl::Replace(const String& word) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         const String& word) {
        if (handler)
          handler->Replace(word);
      },
      main_thread_frame_widget_input_handler_, word));
}

void FrameWidgetInputHandlerImpl::ReplaceMisspelling(const String& word) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         const String& word) {
        if (handler)
          handler->ReplaceMisspelling(word);
      },
      main_thread_frame_widget_input_handler_, word));
}

void FrameWidgetInputHandlerImpl::Delete() {
  RunOnMainThread(base::BindOnce(
      &FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread, widget_,
      main_thread_frame_widget_input_handler_, "Delete", UpdateState::kNone));
}

void FrameWidgetInputHandlerImpl::SelectAll() {
  RunOnMainThread(
      base::BindOnce(&FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread,
                     widget_, main_thread_frame_widget_input_handler_,
                     "SelectAll", UpdateState::kIsSelectingRange));
}

void FrameWidgetInputHandlerImpl::CollapseSelection() {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget,
         base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler) {
        DCHECK_EQ(!!widget, !!handler);
        if (!widget)
          return;
        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        handler->CollapseSelection();
      },
      widget_, main_thread_frame_widget_input_handler_));
}

void FrameWidgetInputHandlerImpl::SelectRange(const gfx::Point& base,
                                              const gfx::Point& extent) {
  // TODO(dtapuska): This event should be coalesced. Chrome IPC uses
  // one outstanding event and an ACK to handle coalescing on the browser
  // side. We should be able to clobber them in the main thread event queue.
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget,
         base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         const gfx::Point& base, const gfx::Point& extent) {
        DCHECK_EQ(!!widget, !!handler);
        if (!widget)
          return;
        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        handler->SelectRange(base, extent);
      },
      widget_, main_thread_frame_widget_input_handler_, base, extent));
}

void FrameWidgetInputHandlerImpl::SelectAroundCaret(
    mojom::blink::SelectionGranularity granularity,
    bool should_show_handle,
    bool should_show_context_menu,
    SelectAroundCaretCallback callback) {
  // If the mojom channel is registered with compositor thread, we have to run
  // the callback on compositor thread. Otherwise run it on main thread. Mojom
  // requires the callback runs on the same thread.
  if (ThreadedCompositingEnabled()) {
    callback = base::BindOnce(
        [](scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
           SelectAroundCaretCallback callback,
           mojom::blink::SelectAroundCaretResultPtr result) {
          callback_task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), std::move(result)));
        },
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(callback));
  }

  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         mojom::blink::SelectionGranularity granularity,
         bool should_show_handle, bool should_show_context_menu,
         SelectAroundCaretCallback callback) {
        if (handler) {
          handler->SelectAroundCaret(granularity, should_show_handle,
                                     should_show_context_menu,
                                     std::move(callback));
        } else {
          std::move(callback).Run(std::move(nullptr));
        }
      },
      main_thread_frame_widget_input_handler_, granularity, should_show_handle,
      should_show_context_menu, std::move(callback)));
}

void FrameWidgetInputHandlerImpl::AdjustSelectionByCharacterOffset(
    int32_t start,
    int32_t end,
    blink::mojom::SelectionMenuBehavior selection_menu_behavior) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget,
         base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         int32_t start, int32_t end,
         blink::mojom::SelectionMenuBehavior selection_menu_behavior) {
        DCHECK_EQ(!!widget, !!handler);
        if (!widget)
          return;
        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        handler->AdjustSelectionByCharacterOffset(start, end,
                                                  selection_menu_behavior);
      },
      widget_, main_thread_frame_widget_input_handler_, start, end,
      selection_menu_behavior));
}

void FrameWidgetInputHandlerImpl::MoveRangeSelectionExtent(
    const gfx::Point& extent) {
  // TODO(dtapuska): This event should be coalesced. Chrome IPC uses
  // one outstanding event and an ACK to handle coalescing on the browser
  // side. We should be able to clobber them in the main thread event queue.
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<WidgetBase> widget,
         base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         const gfx::Point& extent) {
        DCHECK_EQ(!!widget, !!handler);
        if (!widget)
          return;
        HandlingState handling_state(widget, UpdateState::kIsSelectingRange);
        handler->MoveRangeSelectionExtent(extent);
      },
      widget_, main_thread_frame_widget_input_handler_, extent));
}

void FrameWidgetInputHandlerImpl::ScrollFocusedEditableNodeIntoView() {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler) {
        if (handler)
          handler->ScrollFocusedEditableNodeIntoView();
      },
      main_thread_frame_widget_input_handler_));
}

void FrameWidgetInputHandlerImpl::WaitForPageScaleAnimationForTesting(
    WaitForPageScaleAnimationForTestingCallback callback) {
  // Ensure the Mojo callback is invoked from the thread on which the message
  // was received.
  if (ThreadedCompositingEnabled()) {
    callback = base::BindOnce(
        [](scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
           WaitForPageScaleAnimationForTestingCallback callback) {
          callback_task_runner->PostTask(FROM_HERE,
                                         base::BindOnce(std::move(callback)));
        },
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(callback));
  }

  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         WaitForPageScaleAnimationForTestingCallback callback) {
        if (handler)
          handler->WaitForPageScaleAnimationForTesting(std::move(callback));
        else
          std::move(callback).Run();
      },
      main_thread_frame_widget_input_handler_, std::move(callback)));
}

void FrameWidgetInputHandlerImpl::MoveCaret(const gfx::Point& point) {
  RunOnMainThread(base::BindOnce(
      [](base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
         const gfx::Point& point) {
        if (handler)
          handler->MoveCaret(point);
      },
      main_thread_frame_widget_input_handler_, point));
}

void FrameWidgetInputHandlerImpl::ExecuteCommandOnMainThread(
    base::WeakPtr<WidgetBase> widget,
    base::WeakPtr<mojom::blink::FrameWidgetInputHandler> handler,
    const char* command,
    UpdateState update_state) {
  DCHECK_EQ(!!widget, !!handler);
  if (!widget)
    return;
  HandlingState handling_state(widget, update_state);
  handler->ExecuteEditCommand(command, String());
}

FrameWidgetInputHandlerImpl::HandlingState::HandlingState(
    const base::WeakPtr<WidgetBase>& widget,
    UpdateState state)
    : widget_(widget),
      original_select_range_value_(widget->handling_select_range()),
      original_pasting_value_(widget->is_pasting()) {
  switch (state) {
    case UpdateState::kIsPasting:
      widget->set_is_pasting(true);
      [[fallthrough]];  // Set both
    case UpdateState::kIsSelectingRange:
      widget->set_handling_select_range(true);
      break;
    case UpdateState::kNone:
      break;
  }
}

FrameWidgetInputHandlerImpl::HandlingState::~HandlingState() {
  // FrameWidget may have been destroyed while this object was on the stack.
  if (!widget_)
    return;
  widget_->set_handling_select_range(original_select_range_value_);
  widget_->set_is_pasting(original_pasting_value_);
}

}  // namespace blink
