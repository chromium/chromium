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

#include "third_party/blink/renderer/bindings/core/v8/v8_focus_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// static
void HTMLDialogElement::SetFocusForDialog(HTMLDialogElement* dialog) {
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
  document.ClearAXObjectCache();
}

HTMLDialogElement::HTMLDialogElement(Document& document)
    : HTMLElement(html_names::kDialogTag, document),
      is_modal_(false),
      return_value_(""),
      previously_focused_element_(nullptr) {
  UseCounter::Count(document, WebFeature::kDialogElement);
}

void HTMLDialogElement::close(const String& return_value) {
  // https://html.spec.whatwg.org/C/#close-the-dialog

  if (!FastHasAttribute(html_names::kOpenAttr))
    return;
  SetBooleanAttribute(html_names::kOpenAttr, false);
  SetIsModal(false);

  Document& document = GetDocument();
  HTMLDialogElement* old_modal_dialog = document.ActiveModalDialog();
  document.RemoveFromTopLayer(this);
  InertSubtreesChanged(document, old_modal_dialog);

  if (!return_value.IsNull())
    return_value_ = return_value;

  ScheduleCloseEvent();

  // We should call focus() last since it will fire a focus event which could
  // modify this element.
  if (previously_focused_element_) {
    FocusOptions* focus_options = FocusOptions::Create();
    focus_options->setPreventScroll(true);
    Element* previously_focused_element = previously_focused_element_;
    previously_focused_element_ = nullptr;
    previously_focused_element->Focus(focus_options);
  }

  if (close_watcher_) {
    close_watcher_->destroy();
    close_watcher_ = nullptr;
  }
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

void HTMLDialogElement::show() {
  if (FastHasAttribute(html_names::kOpenAttr))
    return;
  SetBooleanAttribute(html_names::kOpenAttr, true);

  // Showing a <dialog> should hide all open popups, immediately.
  if (RuntimeEnabledFeatures::HTMLPopupAttributeEnabled(
          GetDocument().GetExecutionContext())) {
    Element::HideAllPopupsUntil(nullptr, GetDocument(),
                                HidePopupFocusBehavior::kNone,
                                HidePopupForcingLevel::kHideImmediately,
                                HidePopupIndependence::kHideUnrelated);
  }

  // The layout must be updated here because setFocusForDialog calls
  // Element::isFocusable, which requires an up-to-date layout.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  SetFocusForDialog(this);
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

void HTMLDialogElement::showModal(ExceptionState& exception_state) {
  if (FastHasAttribute(html_names::kOpenAttr)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The element already has an 'open' "
                                      "attribute, and therefore cannot be "
                                      "opened modally.");
    return;
  }
  if (!isConnected()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The element is not in a Document.");
    return;
  }

  Document& document = GetDocument();
  HTMLDialogElement* old_modal_dialog = document.ActiveModalDialog();

  // See comment in |Fullscreen::RequestFullscreen|.
  if (Fullscreen::IsInFullscreenElementStack(*this)) {
    UseCounter::Count(document,
                      WebFeature::kShowModalForElementInFullscreenStack);
  }

  // Showing a <dialog> should hide all open popups, immediately.
  if (RuntimeEnabledFeatures::HTMLPopupAttributeEnabled(
          document.GetExecutionContext())) {
    Element::HideAllPopupsUntil(nullptr, document,
                                HidePopupFocusBehavior::kNone,
                                HidePopupForcingLevel::kHideImmediately,
                                HidePopupIndependence::kHideUnrelated);
  }

  document.AddToTopLayer(this);
  SetBooleanAttribute(html_names::kOpenAttr, true);

  SetIsModal(true);

  // Throw away the AX cache first, so the subsequent steps don't have a chance
  // of queuing up AX events on objects that would be invalidated when the cache
  // is thrown away.
  InertSubtreesChanged(document, old_modal_dialog);
  document.UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  if (RuntimeEnabledFeatures::CloseWatcherEnabled()) {
    if (LocalDOMWindow* window = GetDocument().domWindow()) {
      close_watcher_ = CloseWatcher::Create(window);
      if (close_watcher_) {
        auto* event_listener =
            MakeGarbageCollected<DialogCloseWatcherEventListener>(this);
        close_watcher_->addEventListener(event_type_names::kClose,
                                         event_listener);
        close_watcher_->addEventListener(event_type_names::kCancel,
                                         event_listener);
      }
    }
  }

  SetFocusForDialog(this);
}

void HTMLDialogElement::RemovedFrom(ContainerNode& insertion_point) {
  Document& document = GetDocument();
  HTMLDialogElement* old_modal_dialog = document.ActiveModalDialog();
  HTMLElement::RemovedFrom(insertion_point);
  InertSubtreesChanged(document, old_modal_dialog);
  SetIsModal(false);

  if (close_watcher_) {
    close_watcher_->destroy();
    close_watcher_ = nullptr;
  }
}

void HTMLDialogElement::DefaultEventHandler(Event& event) {
  if (!RuntimeEnabledFeatures::CloseWatcherEnabled() &&
      event.type() == event_type_names::kCancel) {
    close();
    event.SetDefaultHandled();
    return;
  }
  HTMLElement::DefaultEventHandler(event);
}

void HTMLDialogElement::CloseWatcherFiredCancel(Event* close_watcher_event) {
  // https://wicg.github.io/close-watcher/#patch-dialog cancelAction

  Event* dialog_event = Event::CreateCancelable(event_type_names::kCancel);
  DispatchEvent(*dialog_event);
  if (dialog_event->defaultPrevented())
    close_watcher_event->preventDefault();
  dialog_event->SetDefaultHandled();
}

void HTMLDialogElement::CloseWatcherFiredClose() {
  // https://wicg.github.io/close-watcher/#patch-dialog closeAction

  close();
}

void HTMLDialogElement::Trace(Visitor* visitor) const {
  visitor->Trace(previously_focused_element_);
  visitor->Trace(close_watcher_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
