/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/html_dialog_element.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_focus_options.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
void HTMLDialogElement::SetFocusForDialogLegacy(HTMLDialogElement* dialog) {
  Element* control = nullptr;
  Node* next = nullptr;

  if (!dialog->isConnected())
    return;

  auto& document = dialog->GetDocument();
  dialog->previously_focused_element_ = document.FocusedElement();

  // TODO(kochi): How to find focusable element inside Shadow DOM is not
  // currently specified.  This may change at any time.
  // See crbug/383230 and https://github.com/whatwg/html/issues/2393 .
  for (Node* node = FlatTreeTraversal::FirstChild(*dialog); node; node = next) {
    next = IsA<HTMLDialogElement>(*node)
               ? FlatTreeTraversal::NextSkippingChildren(*node, dialog)
               : FlatTreeTraversal::Next(*node, dialog);

    auto* element = DynamicTo<Element>(node);
    if (!element)
      continue;
    if (element->IsAutofocusable() && element->IsFocusable()) {
      control = element;
      break;
    }
    if (!control && element->IsFocusable())
      control = element;
  }
  if (!control)
    control = dialog;

  // 3. Run the focusing steps for control.
  if (control->IsFocusable())
    control->Focus();
  else
    document.ClearFocusedElement();

  // 4. Let topDocument be the active document of control's node document's
  // browsing context's top-level browsing context.
  // 5. If control's node document's origin is not the same as the origin of
  // topDocument, then return.
  Document& doc = control->GetDocument();
  if (!doc.IsActive())
    return;
  if (!doc.IsInMainFrame() &&
      !doc.TopFrameOrigin()->CanAccess(
          doc.GetExecutionContext()->GetSecurityOrigin())) {
    return;
  }

  // 6. Empty topDocument's autofocus candidates.
  // 7. Set topDocument's autofocus processed flag to true.
  doc.TopDocument().FinalizeAutofocus();
}

static void InertSubtreesChanged(Document& document,
                                 Element* old_modal_dialog) {
  Element* new_modal_dialog = document.ActiveModalDialog();
  if (old_modal_dialog == new_modal_dialog)
    return;

  // Update IsInert() flags.
  const StyleChangeReasonForTracing& reason =
      StyleChangeReasonForTracing::Create(style_change_reason::kDialog);
  if (old_modal_dialog && new_modal_dialog) {
    old_modal_dialog->SetNeedsStyleRecalc(kLocalStyleChange, reason);
    new_modal_dialog->SetNeedsStyleRecalc(kLocalStyleChange, reason);
  } else {
    if (Element* root = document.documentElement())
      root->SetNeedsStyleRecalc(kLocalStyleChange, reason);
    if (Element* fullscreen = Fullscreen::FullscreenElementFrom(document))
      fullscreen->SetNeedsStyleRecalc(kLocalStyleChange, reason);
  }

  // When a modal dialog opens or closes, nodes all over the accessibility
  // tree can change inertness which means they must be added or removed from
  // the tree. The most foolproof way is to clear the entire tree and rebuild
  // it, though a more clever way is probably possible.
  document.RefreshAccessibilityTree();
}

HTMLDialogElement::HTMLDialogElement(Document& document)
    : HTMLElement(html_names::kDialogTag, document),
      is_modal_(false),
      return_value_(""),
      request_close_return_value_(""),
      previously_focused_element_(nullptr) {
  UseCounter::Count(document, WebFeature::kDialogElement);
}

void HTMLDialogElement::close(const String& return_value,
                              Element* invoker,
                              bool open_attribute_being_removed) {
  DCHECK(!open_attribute_being_removed ||
         RuntimeEnabledFeatures::DialogCloseWhenOpenRemovedEnabled());
  // https://html.spec.whatwg.org/C/#close-the-dialog
  if (is_closing_) {
    return;
  }

  bool was_modal;
  {
    // The `is_closing_` variable avoids doing more work in the attribute change
    // steps triggered by `SetBooleanAttribute(kOpenAttr)`.
    base::AutoReset<bool> reset_close(&is_closing_, true);

    if (!IsOpen() && !open_attribute_being_removed) {
      return;
    }

    Document& document = GetDocument();
    HTMLDialogElement* old_modal_dialog = document.ActiveModalDialog();

    DispatchToggleEvents(/*opening=*/false, invoker);
    if (!IsOpen() && !open_attribute_being_removed) {
      return;
    }
    SetBooleanAttribute(html_names::kOpenAttr, false);
    was_modal = IsModal();
    SetIsModal(false);
    // Because we set `is_closing_` above, the `open` attribute setter will not
    // run its actions. We need to therefore manage the open dialogs list here.
    if (isConnected()) {
      DCHECK(GetDocument().AllOpenDialogs().Contains(this));
      GetDocument().AllOpenDialogs().erase(this);
    }

    // If this dialog is open as a non-modal dialog and open as a popover at the
    // same time, then we shouldn't remove it from the top layer because it is
    // still open as a popover.
    if (was_modal) {
      document.ScheduleForTopLayerRemoval(this,
                                          Document::TopLayerReason::kDialog);
    }
    InertSubtreesChanged(document, old_modal_dialog);

    if (!return_value.IsNull()) {
      return_value_ = return_value;
    }

    ScheduleCloseEvent();

    if (close_watcher_) {
      close_watcher_->destroy();
      close_watcher_ = nullptr;
    }
  }  // End of is_closing_ reset scope

  // We should call focus() last since it will fire a focus event which could
  // modify this element.
  if (previously_focused_element_ && !open_attribute_being_removed) {
    Element* previously_focused_element = previously_focused_element_;
    previously_focused_element_ = nullptr;

    bool descendant_is_focused = GetDocument().FocusedElement() &&
                                 FlatTreeTraversal::IsDescendantOf(
                                     *GetDocument().FocusedElement(), *this);
    if (previously_focused_element && (was_modal || descendant_is_focused)) {
      FocusOptions* focus_options = FocusOptions::Create();
      focus_options->setPreventScroll(true);
      previously_focused_element->Focus(FocusParams(
          SelectionBehaviorOnFocus::kNone, mojom::blink::FocusType::kScript,
          nullptr, focus_options));
    }
  }
}

void HTMLDialogElement::RequestCloseInternal(const String& return_value,
                                             Element* invoker,
                                             ExceptionState& exception_state) {
  if (!IsOpenAndActive()) {
    return;
  }
  CHECK(close_watcher_);
  close_watcher_->setEnabled(true);
  request_close_return_value_ = return_value;
  request_close_source_element_ = invoker;
  close_watcher_->RequestClose(CloseWatcher::AllowCancel::kAlways);
  SetCloseWatcherEnabledState();
}

ClosedByState HTMLDialogElement::ClosedBy() const {
  auto attribute_value =
      FastGetAttribute(html_names::kClosedbyAttr).LowerASCII();
  if (attribute_value == keywords::kAny) {
    return ClosedByState::kAny;
  } else if (attribute_value == keywords::kNone) {
    return ClosedByState::kNone;
  } else if (attribute_value == keywords::kCloserequest) {
    return ClosedByState::kCloseRequest;
  } else {
    // The closedby attribute's invalid value default and missing value default
    // are both the Auto state. The Auto state matches closerequest when the
    // element is modal; otherwise none.
    return IsModal() ? ClosedByState::kCloseRequest : ClosedByState::kNone;
  }
}

String HTMLDialogElement::closedBy() const {
  switch (ClosedBy()) {
    case ClosedByState::kAny:
      return keywords::kAny;
    case ClosedByState::kCloseRequest:
      return keywords::kCloserequest;
    case ClosedByState::kNone:
      return keywords::kNone;
  }
}

void HTMLDialogElement::setClosedBy(const String& new_value) {
  setAttribute(html_names::kClosedbyAttr, AtomicString(new_value));
}

namespace {

const HTMLDialogElement* FindNearestDialog(const Node& target_node,
                                           double client_x,
                                           double client_y) {
  // First check if this is a click on a dialog's backdrop, which will show up
  // as a click on the dialog directly.
  if (auto* dialog = DynamicTo<HTMLDialogElement>(target_node);
      dialog && dialog->IsOpenAndActive() && dialog->IsModal()) {
    DOMRect* dialog_rect =
        const_cast<HTMLDialogElement*>(dialog)->GetBoundingClientRect();
    if (!dialog_rect->IsPointInside(client_x, client_y)) {
      return nullptr;  // Return nullptr for a backdrop click.
    }
  }
  // Otherwise, walk up the tree looking for an open dialog.
  for (const Node* node = &target_node; node;
       node = FlatTreeTraversal::Parent(*node)) {
    if (auto* dialog = DynamicTo<HTMLDialogElement>(node);
        dialog && dialog->IsOpenAndActive()) {
      return dialog;
    }
  }
  return nullptr;
}

}  // namespace

// static
// https://html.spec.whatwg.org/interactive-elements.html#light-dismiss-open-dialogs
void HTMLDialogElement::HandleDialogLightDismiss(
    const PointerEvent& pointer_event,
    const Node& target_node) {
  CHECK(!RuntimeEnabledFeatures::LightDismissFromClickEnabled());
  CHECK(pointer_event.isTrusted());
  // PointerEventManager will call this function before actually dispatching
  // the event.
  CHECK(!pointer_event.HasEventPath());
  CHECK_EQ(Event::PhaseType::kNone, pointer_event.eventPhase());

  // If there aren't any open dialogs, there's nothing to light dismiss.
  auto& document = target_node.GetDocument();
  if (document.AllOpenDialogs().empty()) {
    return;
  }

  const AtomicString& event_type = pointer_event.type();
  const HTMLDialogElement* ancestor_dialog = FindNearestDialog(
      target_node, pointer_event.clientX(), pointer_event.clientY());
  if (event_type == event_type_names::kPointerdown) {
    document.SetDialogPointerdownTarget(ancestor_dialog);
  } else if (event_type == event_type_names::kPointerup) {
    // See the comment in HTMLElement::HandlePopoverLightDismiss() for details
    // on why this works the way it does.
    bool same_target = ancestor_dialog == document.DialogPointerdownTarget();
    document.SetDialogPointerdownTarget(nullptr);
    if (!same_target) {
      return;
    }
    HTMLDialogElement* topmost_dialog = document.AllOpenDialogs().back();
    if (ancestor_dialog == topmost_dialog) {
      return;
    }
    if (topmost_dialog->ClosedBy() == ClosedByState::kAny) {
      topmost_dialog->requestClose(String(), ASSERT_NO_EXCEPTION);
    }
  }
}

// static
// https://html.spec.whatwg.org/interactive-elements.html#light-dismiss-open-dialogs
void HTMLDialogElement::HandleDialogLightDismissForClick(
    const PointerEventFactory::PointerTarget& pointer_down_target,
    const PointerEventFactory::PointerTarget& pointer_up_target) {
  CHECK(RuntimeEnabledFeatures::LightDismissFromClickEnabled());

  // If there aren't any open dialogs, there's nothing to light dismiss.
  auto& document = pointer_down_target.node->GetDocument();
  if (document.AllOpenDialogs().empty()) {
    return;
  }

  const HTMLDialogElement* pointer_down_dialog =
      FindNearestDialog(*pointer_down_target.node, pointer_down_target.client_x,
                        pointer_down_target.client_y);
  const HTMLDialogElement* pointer_up_dialog =
      FindNearestDialog(*pointer_up_target.node, pointer_up_target.client_x,
                        pointer_up_target.client_y);
  if (pointer_down_dialog == pointer_up_dialog) {
    HTMLDialogElement* topmost_dialog = document.AllOpenDialogs().back();
    if (pointer_down_dialog == topmost_dialog) {
      return;
    }
    if (topmost_dialog->ClosedBy() == ClosedByState::kAny) {
      topmost_dialog->requestClose(String(), ASSERT_NO_EXCEPTION);
    }
  }
}

bool HTMLDialogElement::IsValidBuiltinCommand(HTMLElement& invoker,
                                              CommandEventType command) {
  return HTMLElement::IsValidBuiltinCommand(invoker, command) ||
         command == CommandEventType::kShowModal ||
         command == CommandEventType::kClose ||
         (command == CommandEventType::kRequestClose &&
          RuntimeEnabledFeatures::HTMLCommandRequestCloseEnabled());
}

bool HTMLDialogElement::HandleCommandInternal(HTMLElement& invoker,
                                              CommandEventType command) {
  if (!IsValidBuiltinCommand(invoker, command)) {
    return false;
  }
  if (HTMLElement::HandleCommandInternal(invoker, command)) {
    return true;
  }

  // Dialog actions conflict with popovers. We should avoid trying do anything
  // with a dialog that is an open popover.
  if (IsPopover() && popoverOpen()) {
    AddConsoleMessage(mojom::blink::ConsoleMessageSource::kOther,
                      mojom::blink::ConsoleMessageLevel::kError,
                      "Dialog commands are ignored on open popovers.");
    return false;
  }

  bool open = IsOpenAndActive();
  String return_value;

  if (command == CommandEventType::kClose ||
      command == CommandEventType::kRequestClose) {
    if (auto* invokerButton = DynamicTo<HTMLButtonElement>(invoker)) {
      return_value = invokerButton->Value();
    }
  }

  if (command == CommandEventType::kClose) {
    if (open) {
      close(return_value, &invoker);
      return true;
    } else {
      AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "A command attempted to close an already closed Dialog");
    }
  } else if (command == CommandEventType::kRequestClose) {
    CHECK(RuntimeEnabledFeatures::HTMLCommandRequestCloseEnabled());
    if (open) {
      RequestCloseInternal(return_value, &invoker, ASSERT_NO_EXCEPTION);
      return true;
    } else {
      AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "A command attempted to request to close an already closed Dialog");
    }
  } else if (command == CommandEventType::kShowModal) {
    if (isConnected() && !open) {
      showModal(ASSERT_NO_EXCEPTION, &invoker);
      return true;
    } else {
      AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "A command attempted to open an already open Dialog as a modal");
    }
  }

  return false;
}

void HTMLDialogElement::SetIsModal(bool is_modal) {
  if (is_modal != is_modal_)
    PseudoStateChanged(CSSSelector::kPseudoModal);
  is_modal_ = is_modal;
}

void HTMLDialogElement::ScheduleCloseEvent() {
  Event* event = Event::Create(event_type_names::kClose);
  event->SetTarget(this);
  GetDocument().EnqueueAnimationFrameEvent(event);
}

void HTMLDialogElement::show(ExceptionState& exception_state) {
  if (IsOpen()) {
    if (IsModal()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The dialog is already open as a modal dialog, and therefore "
          "cannot be opened as a non-modal dialog.");
    }
    return;
  }

  if (!DispatchToggleEvents(/*opening=*/true, /*invoker=*/nullptr)) {
    return;
  }
  SetBooleanAttribute(html_names::kOpenAttr, true);

  // The layout must be updated here because setFocusForDialog calls
  // Element::isFocusable, which requires an up-to-date layout.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  // Top layer elements like dialogs and fullscreen elements can be nested
  // inside popovers.
  auto* hide_until = HTMLElement::TopLayerElementPopoverAncestor(
      *this, TopLayerElementType::kDialog);
  HTMLElement::HideAllPopoversUntil(
      hide_until, GetDocument(), HidePopoverFocusBehavior::kNone,
      HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions);

  if (RuntimeEnabledFeatures::DialogNewFocusBehaviorEnabled()) {
    SetFocusForDialog();
  } else {
    SetFocusForDialogLegacy(this);
  }
}

bool HTMLDialogElement::IsKeyboardFocusableSlow(
    UpdateBehavior update_behavior) const {
  if (!IsFocusable(update_behavior)) {
    return false;
  }
  // This handles cases such as <dialog tabindex=0>, <dialog contenteditable>,
  // etc.
  return Element::SupportsFocus(update_behavior) !=
             FocusableState::kNotFocusable &&
         GetIntegralAttribute(html_names::kTabindexAttr, 0) >= 0;
}

class DialogCloseWatcherEventListener : public NativeEventListener {
 public:
  explicit DialogCloseWatcherEventListener(HTMLDialogElement* dialog)
      : dialog_(dialog) {}

  void Invoke(ExecutionContext*, Event* event) override {
    if (!dialog_)
      return;
    if (event->type() == event_type_names::kCancel)
      dialog_->CloseWatcherFiredCancel(event);
    if (event->type() == event_type_names::kClose)
      dialog_->CloseWatcherFiredClose();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(dialog_);
    NativeEventListener::Trace(visitor);
  }

 private:
  WeakMember<HTMLDialogElement> dialog_;
};

void HTMLDialogElement::SetCloseWatcherEnabledState() {
  if (!IsOpenAndActive()) {
    return;
  }
  CHECK(close_watcher_);
  ClosedByState closed_by = ClosedBy();
  close_watcher_->setEnabled(closed_by != ClosedByState::kNone);
}

void HTMLDialogElement::CreateCloseWatcher() {
  DCHECK(!close_watcher_);
  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window) {
    return;
  }
  CHECK(IsOpenAndActive());
  CHECK(window->GetFrame());
  close_watcher_ = CloseWatcher::Create(*window);
  CHECK(close_watcher_);
  SetCloseWatcherEnabledState();
  auto* event_listener =
      MakeGarbageCollected<DialogCloseWatcherEventListener>(this);
  close_watcher_->addEventListener(event_type_names::kClose, event_listener);
  close_watcher_->addEventListener(event_type_names::kCancel, event_listener);
}

void HTMLDialogElement::showModal(ExceptionState& exception_state,
                                  Element* invoker) {
  if (IsOpen()) {
    if (!IsModal()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "The dialog is already open as a non-modal dialog, and therefore "
          "cannot be opened as a modal dialog.");
    }
    return;
  }
  if (!isConnected()) {
    return exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The element is not in a Document.");
  }
  if (IsPopover() && popoverOpen()) {
    return exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The dialog is already open as a Popover, and therefore cannot be "
        "opened as a modal dialog.");
  }
  if (!GetDocument().IsActive()) {
    return exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Invalid for dialogs within documents that are not fully active.");
  }
  if (!DispatchToggleEvents(/*opening=*/true, invoker, /*asModal=*/true)) {
    return;
  }

  Document& document = GetDocument();
  HTMLDialogElement* old_modal_dialog = document.ActiveModalDialog();

  // See comment in |Fullscreen::RequestFullscreen|.
  if (Fullscreen::IsInFullscreenElementStack(*this)) {
    UseCounter::Count(document,
                      WebFeature::kShowModalForElementInFullscreenStack);
  }

  document.AddToTopLayer(this);
  SetIsModal(true);
  SetBooleanAttribute(html_names::kOpenAttr, true);

  // Refresh the AX cache first, because most of it is changing.
  InertSubtreesChanged(document, old_modal_dialog);
  document.UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  // Setting the open attribute already created the close watcher.
  DCHECK(close_watcher_);
  SetCloseWatcherEnabledState();

  // Top layer elements like dialogs and fullscreen elements can be nested
  // inside popovers.
  auto* hide_until = HTMLElement::TopLayerElementPopoverAncestor(
      *this, TopLayerElementType::kDialog);
  HTMLElement::HideAllPopoversUntil(
      hide_until, document, HidePopoverFocusBehavior::kNone,
      HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions);

  if (RuntimeEnabledFeatures::DialogNewFocusBehaviorEnabled()) {
    SetFocusForDialog();
  } else {
    SetFocusForDialogLegacy(this);
  }
}

Node::InsertionNotificationRequest HTMLDialogElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);

  if (FastHasAttribute(html_names::kOpenAttr) &&
      insertion_point.isConnected() &&
      !GetDocument().StatePreservingAtomicMoveInProgress()) {
    DCHECK(!GetDocument().AllOpenDialogs().Contains(this));
    GetDocument().AllOpenDialogs().insert(this);
    CreateCloseWatcher();
  }
  return kInsertionDone;
}

void HTMLDialogElement::RemovedFrom(ContainerNode& insertion_point) {
  Document& document = GetDocument();
  HTMLDialogElement* old_modal_dialog = document.ActiveModalDialog();
  HTMLElement::RemovedFrom(insertion_point);
  InertSubtreesChanged(document, old_modal_dialog);

  if (GetDocument().StatePreservingAtomicMoveInProgress() ||
      !insertion_point.isConnected()) {
    return;
  }

  SetIsModal(false);
  document.RemoveFromTopLayerImmediately(this);
  GetDocument().AllOpenDialogs().erase(this);
  if (close_watcher_) {
    close_watcher_->destroy();
    close_watcher_ = nullptr;
  }
}

void HTMLDialogElement::CloseWatcherFiredCancel(Event* close_watcher_event) {
  // https://wicg.github.io/close-watcher/#patch-dialog cancelAction

  Event* dialog_event = close_watcher_event->cancelable()
                            ? Event::CreateCancelable(event_type_names::kCancel)
                            : Event::Create(event_type_names::kCancel);
  DispatchEvent(*dialog_event);
  if (dialog_event->defaultPrevented())
    close_watcher_event->preventDefault();
  dialog_event->SetDefaultHandled();
}

void HTMLDialogElement::CloseWatcherFiredClose() {
  // https://wicg.github.io/close-watcher/#patch-dialog closeAction

  close(request_close_return_value_, request_close_source_element_);
  request_close_source_element_ = nullptr;
}

// https://html.spec.whatwg.org#dialog-focusing-steps
void HTMLDialogElement::SetFocusForDialog() {
  previously_focused_element_ = GetDocument().FocusedElement();

  Element* control = GetFocusDelegate(/*autofocus_only=*/false);
  if (IsAutofocusable()) {
    control = this;
  }
  if (!control) {
    control = this;
  }

  if (control->IsFocusable()) {
    control->Focus();
  } else if (IsModal()) {
    control->GetDocument().ClearFocusedElement();
  }

  // 4. Let topDocument be the active document of control's node document's
  // browsing context's top-level browsing context.
  // 5. If control's node document's origin is not the same as the origin of
  // topDocument, then return.
  Document& doc = control->GetDocument();
  if (!doc.IsActive())
    return;
  if (!doc.IsInMainFrame() &&
      !doc.TopFrameOrigin()->CanAccess(
          doc.GetExecutionContext()->GetSecurityOrigin())) {
    return;
  }

  // 6. Empty topDocument's autofocus candidates.
  // 7. Set topDocument's autofocus processed flag to true.
  doc.TopDocument().FinalizeAutofocus();
}

// Returns false if beforetoggle was canceled, otherwise true. Queues a toggle
// event if beforetoggle was not canceled.
bool HTMLDialogElement::DispatchToggleEvents(bool opening,
                                             Element* source,
                                             bool asModal) {
  String old_state = opening ? "closed" : "open";
  String new_state = opening ? "open" : "closed";

  if (DispatchEvent(*ToggleEvent::Create(
          event_type_names::kBeforetoggle,
          opening ? Event::Cancelable::kYes : Event::Cancelable::kNo, old_state,
          new_state, source)) != DispatchEventResult::kNotCanceled) {
    return false;
  }
  if (opening) {
    if (IsOpen()) {
      return false;
    }
    if (asModal && (!isConnected() || (IsPopover() && popoverOpen()))) {
      return false;
    }
  }

  if (pending_toggle_event_) {
    old_state = pending_toggle_event_->oldState();
  }
  pending_toggle_event_ =
      ToggleEvent::Create(event_type_names::kToggle, Event::Cancelable::kNo,
                          old_state, new_state, source);
  pending_toggle_event_task_ = PostCancellableTask(
      *GetDocument().GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      BindOnce(&HTMLDialogElement::DispatchPendingToggleEvent,
               WrapPersistent(this)));
  return true;
}

void HTMLDialogElement::DispatchPendingToggleEvent() {
  if (!pending_toggle_event_) {
    return;
  }
  DispatchEvent(*pending_toggle_event_);
  pending_toggle_event_ = nullptr;
}

void HTMLDialogElement::Trace(Visitor* visitor) const {
  visitor->Trace(request_close_source_element_);
  visitor->Trace(previously_focused_element_);
  visitor->Trace(close_watcher_);
  visitor->Trace(pending_toggle_event_);
  HTMLElement::Trace(visitor);
}

void HTMLDialogElement::AttributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::AttributeChanged(params);
  if (params.name == html_names::kClosedbyAttr) {
    UseCounter::CountWebDXFeature(GetDocument(), WebDXFeature::kDialogClosedby);
    if (IsOpenAndActive() && params.old_value != params.new_value) {
      SetCloseWatcherEnabledState();
    }
  }
  if (params.name == html_names::kOpenAttr &&
      params.old_value != params.new_value) {
    PseudoStateChanged(CSSSelector::kPseudoOpen);
  }
}

void HTMLDialogElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kOpenAttr && !is_closing_) {
    if (params.new_value.IsNull()) {
      // The open attribute has been removed explicitly, without calling
      // close().
      if (RuntimeEnabledFeatures::DialogCloseWhenOpenRemovedEnabled()) {
        AddConsoleMessage(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "The open attribute was removed from a dialog element while it was "
            "open. This is not recommended - some closing behaviors will not "
            "occur. Please close dialogs using dialog.close().");
        close(/*return_value=*/String(), /*invoker=*/nullptr,
              /*open_attribute_being_removed=*/true);
      } else {
        DCHECK(GetDocument().AllOpenDialogs().Contains(this));
        GetDocument().AllOpenDialogs().erase(this);
        if (close_watcher_) {
          close_watcher_->destroy();
          close_watcher_ = nullptr;
        }
      }
    } else if (params.old_value.IsNull() && isConnected()) {
      // The `open` attribute is being added, and the element is already
      // connected. We need to ensure there's a closewatcher created and the
      // open dialogs list is up to date. If the element isn't connected, then
      // these updates will be performed when it gets inserted.
      DCHECK(!GetDocument().AllOpenDialogs().Contains(this));
      GetDocument().AllOpenDialogs().insert(this);
      CreateCloseWatcher();
    }
  }

  HTMLElement::ParseAttribute(params);
}

}  // namespace blink
