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

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

using namespace HTMLNames;

// This function chooses the focused element when show() or showModal() is
// invoked, as described in their spec.
static void SetFocusForDialog(HTMLDialogElement* dialog) {
  Element* focusable_descendant = nullptr;
  Node* next = nullptr;

  // TODO(kochi): How to find focusable element inside Shadow DOM is not
  // currently specified.  This may change at any time.
  // See crbug/383230 and https://github.com/whatwg/html/issues/2393 .
  for (Node* node = FlatTreeTraversal::FirstChild(*dialog); node; node = next) {
    next = IsHTMLDialogElement(*node)
               ? FlatTreeTraversal::NextSkippingChildren(*node, dialog)
               : FlatTreeTraversal::Next(*node, dialog);

    if (!node->IsElementNode())
      continue;
    Element* element = ToElement(node);
    if (element->IsFormControlElement()) {
      HTMLFormControlElement* control = ToHTMLFormControlElement(node);
      if (control->IsAutofocusable() && control->IsFocusable()) {
        control->focus();
        return;
      }
    }
    if (!focusable_descendant && element->IsFocusable())
      focusable_descendant = element;
  }

  if (focusable_descendant) {
    focusable_descendant->focus();
    return;
  }

  if (dialog->IsFocusable()) {
    dialog->focus();
    return;
  }

  dialog->GetDocument().ClearFocusedElement();
}

static void InertSubtreesChanged(Document& document) {
  // SetIsInert recurses through subframes to propagate the inert bit.
  if (document.GetFrame()) {
    document.GetFrame()->SetIsInert(document.LocalOwner() &&
                                    document.LocalOwner()->IsInert());
  }

  // When a modal dialog opens or closes, nodes all over the accessibility
  // tree can change inertness which means they must be added or removed from
  // the tree. The most foolproof way is to clear the entire tree and rebuild
  // it, though a more clever way is probably possible.
  document.ClearAXObjectCache();
}

inline HTMLDialogElement::HTMLDialogElement(Document& document)
    : HTMLElement(dialogTag, document),
      centering_mode_(kNotCentered),
      centered_position_(0),
      return_value_("") {
  UseCounter::Count(document, WebFeature::kDialogElement);
}

DEFINE_NODE_FACTORY(HTMLDialogElement)

void HTMLDialogElement::close(const String& return_value) {
  // https://html.spec.whatwg.org/#close-the-dialog

  if (!FastHasAttribute(openAttr))
    return;
  SetBooleanAttribute(openAttr, false);

  HTMLDialogElement* active_modal_dialog = GetDocument().ActiveModalDialog();
  GetDocument().RemoveFromTopLayer(this);
  if (active_modal_dialog == this)
    InertSubtreesChanged(GetDocument());

  if (!return_value.IsNull())
    return_value_ = return_value;

  ScheduleCloseEvent();
}

void HTMLDialogElement::ForceLayoutForCentering() {
  centering_mode_ = kNeedsCentering;
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (centering_mode_ == kNeedsCentering)
    SetNotCentered();
}

void HTMLDialogElement::ScheduleCloseEvent() {
  Event* event = Event::Create(EventTypeNames::close);
  event->SetTarget(this);
  GetDocument().EnqueueAnimationFrameEvent(event);
}

void HTMLDialogElement::show() {
  if (FastHasAttribute(openAttr))
    return;
  SetBooleanAttribute(openAttr, true);

  // The layout must be updated here because setFocusForDialog calls
  // Element::isFocusable, which requires an up-to-date layout.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  SetFocusForDialog(this);
}

void HTMLDialogElement::showModal(ExceptionState& exception_state) {
  if (FastHasAttribute(openAttr)) {
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

  // See comment in |Fullscreen::RequestFullscreen|.
  if (Fullscreen::IsInFullscreenElementStack(*this)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kShowModalForElementInFullscreenStack);
  }

  GetDocument().AddToTopLayer(this);
  SetBooleanAttribute(openAttr, true);

  ForceLayoutForCentering();

  // Throw away the AX cache first, so the subsequent steps don't have a chance
  // of queuing up AX events on objects that would be invalidated when the cache
  // is thrown away.
  InertSubtreesChanged(GetDocument());

  SetFocusForDialog(this);
}

void HTMLDialogElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);
  SetNotCentered();
  InertSubtreesChanged(GetDocument());
}

void HTMLDialogElement::SetCentered(LayoutUnit centered_position) {
  DCHECK_EQ(centering_mode_, kNeedsCentering);
  centered_position_ = centered_position;
  centering_mode_ = kCentered;
}

void HTMLDialogElement::SetNotCentered() {
  centering_mode_ = kNotCentered;
}

bool HTMLDialogElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  // FIXME: Workaround for <https://bugs.webkit.org/show_bug.cgi?id=91058>:
  // modifying an attribute for which there is an attribute selector in html.css
  // sometimes does not trigger a style recalc.
  if (name == openAttr)
    return true;

  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLDialogElement::DefaultEventHandler(Event& event) {
  if (event.type() == EventTypeNames::cancel) {
    close();
    event.SetDefaultHandled();
    return;
  }
  HTMLElement::DefaultEventHandler(event);
}

}  // namespace blink
