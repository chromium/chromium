// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/dictation/dictation_agent_impl.h"

#include "base/time/time.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/ime/text_update_event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

namespace {

// Some editors keep only this text in their EditContext, with the caret after
// it, until they see IME input.
constexpr char kPlaceholderText[] = "_";

bool IsInPlaceholderState(const EditContext& edit_context) {
  return edit_context.text() == kPlaceholderText &&
         edit_context.selectionStart() == 1 && edit_context.selectionEnd() == 1;
}

}  // namespace

// static
const char DictationAgentImpl::kSupplementName[] = "DictationAgentImpl";

// static
DictationAgentImpl* DictationAgentImpl::CreateIfNeeded(Document& document) {
  if (!document.IsActive()) {
    return nullptr;
  }
  auto* agent = Supplement<Document>::From<DictationAgentImpl>(document);
  if (!agent) {
    agent = MakeGarbageCollected<DictationAgentImpl>(document, PassKey());
    Supplement<Document>::ProvideTo(document, agent);
  }
  return agent;
}

// static
void DictationAgentImpl::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::DictationAgent> receiver) {
  DCHECK(frame && frame->GetDocument());
  if (DictationAgentImpl* agent = CreateIfNeeded(*frame->GetDocument())) {
    agent->Bind(std::move(receiver));
  }
}

DictationAgentImpl::DictationAgentImpl(Document& document, PassKey)
    : Supplement<Document>(document),
      receiver_(this, document.GetExecutionContext()) {}

void DictationAgentImpl::Bind(
    mojo::PendingReceiver<mojom::blink::DictationAgent> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver), GetSupplementable()->GetTaskRunner(
                                          TaskType::kInternalUserInteraction));
}

void DictationAgentImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  Supplement<Document>::Trace(visitor);
}

HTMLElement* DictationAgentImpl::GetTarget(int32_t target_dom_node_id) const {
  // The target node may already be gone by the time the renderer receives the
  // IPC from the browser, or the node itself may have been moved into
  // another document.
  auto* target =
      DynamicTo<HTMLElement>(DOMNodeIds::NodeForId(target_dom_node_id));
  if (!target || &target->GetDocument() != GetSupplementable()) {
    return nullptr;
  }
  return target;
}

EditContext* DictationAgentImpl::GetTargetEditContext(
    HTMLElement& target) const {
  // A detached document has no focused element, so this also returns early
  // before touching the frame if script detached it.
  if (GetSupplementable()->FocusedElement() != &target) {
    return nullptr;
  }
  EditContext* edit_context = target.editContext();
  if (!edit_context) {
    return nullptr;
  }
  const InputMethodController& controller =
      GetSupplementable()->GetFrame()->GetInputMethodController();
  return controller.GetActiveEditContext() == edit_context ? edit_context
                                                           : nullptr;
}

void DictationAgentImpl::PopulateEditContext(
    int32_t target_dom_node_id,
    PopulateEditContextCallback callback) {
  HTMLElement* target = GetTarget(target_dom_node_id);
  EditContext* edit_context = target ? GetTargetEditContext(*target) : nullptr;
  if (did_populate_edit_context_ || !edit_context ||
      !IsInPlaceholderState(*edit_context)) {
    std::move(callback).Run();
    return;
  }
  did_populate_edit_context_ = true;

  // A keydown with keyCode 229 (VKEY_PROCESSKEY), which pages treat as IME
  // input. It is dispatched to the element only, not through the keyboard
  // pipeline, so it has no default editing action.
  WebKeyboardEvent key_down(WebInputEvent::Type::kRawKeyDown,
                            WebInputEvent::kNoModifiers,
                            base::TimeTicks::Now());
  key_down.windows_key_code = VKEY_PROCESSKEY;
  target->DispatchEvent(
      *KeyboardEvent::Create(key_down, GetSupplementable()->domWindow()));

  // An empty textupdate at the caret, in the same task. The keydown above only
  // tells the editor that IME input started; this textupdate, which changes no
  // text, is what makes it fill its EditContext with its real text and
  // selection. The keydown handler may have run script, so only send it if the
  // EditContext is still active, and use its current selection in case the
  // script changed it. It is fine to send it even if the EditContext has
  // already left the placeholder state, since it changes nothing.
  if (edit_context == GetTargetEditContext(*target)) {
    const uint32_t selection_start = edit_context->selectionStart();
    const uint32_t selection_end = edit_context->selectionEnd();
    edit_context->DispatchEvent(*MakeGarbageCollected<TextUpdateEvent>(
        event_type_names::kTextupdate, g_empty_string,
        /*update_range_start=*/selection_end,
        /*update_range_end=*/selection_end, selection_start, selection_end));
  }
  std::move(callback).Run();
}

}  // namespace blink
