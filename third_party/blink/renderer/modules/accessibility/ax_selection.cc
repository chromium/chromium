// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

namespace {

// TODO(nektar): Add Web tests for this event.
void ScheduleSelectEvent(TextControlElement& text_control) {
  Event* event = Event::CreateBubble(event_type_names::kSelect);
  event->SetTarget(&text_control);
  text_control.GetDocument().EnqueueAnimationFrameEvent(event);
}

// TODO(nektar): Add Web tests for this event.
DispatchEventResult DispatchSelectStart(Node* node) {
  if (!node)
    return DispatchEventResult::kNotCanceled;

  return node->DispatchEvent(
      *Event::CreateCancelableBubble(event_type_names::kSelectstart));
}

}  // namespace

//
// AXSelection::Builder
//

AXSelection::Builder& AXSelection::Builder::SetAnchor(
    const AXPosition& anchor) {
  DCHECK(anchor.IsValid());
  selection_.anchor_ = anchor;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetAnchor(const Position& anchor) {
  const auto ax_anchor = AXPosition::FromPosition(anchor);
  DCHECK(ax_anchor.IsValid());
  selection_.anchor_ = ax_anchor;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetFocus(const AXPosition& focus) {
  DCHECK(focus.IsValid());
  selection_.focus_ = focus;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetFocus(const Position& focus) {
  const auto ax_focus = AXPosition::FromPosition(focus);
  DCHECK(ax_focus.IsValid());
  selection_.focus_ = ax_focus;
  return *this;
}

AXSelection::Builder& AXSelection::Builder::SetSelection(
    const SelectionInDOMTree& selection) {
  if (selection.IsNone())
    return *this;

  selection_.anchor_ = AXPosition::FromPosition(selection.Anchor());
  selection_.focus_ = AXPosition::FromPosition(selection.Focus());
  return *this;
}

const AXSelection AXSelection::Builder::Build() {
  if (!selection_.Anchor().IsValid() || !selection_.Focus().IsValid()) {
    return {};
  }

  const Document* document =
      selection_.Anchor().ContainerObject()->GetDocument();
  DCHECK(document);
  DCHECK(document->IsActive());
  DCHECK(!document->NeedsLayoutTreeUpdate());
  // We don't support selections that span across documents.
  if (selection_.Focus().ContainerObject()->GetDocument() != document) {
    return {};
  }

#if DCHECK_IS_ON()
  selection_.dom_tree_version_ = document->DomTreeVersion();
  selection_.style_version_ = document->StyleVersion();
#endif
  return selection_;
}

//
// AXSelection
//

// static
void AXSelection::ClearCurrentSelection(Document& document) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return;

  FrameSelection& frame_selection = frame->Selection();
  if (!frame_selection.IsAvailable())
    return;

  frame_selection.Clear();
}

// static
AXSelection AXSelection::FromCurrentSelection(
    const Document& document,
    const AXSelectionBehavior selection_behavior) {
  const LocalFrame* frame = document.GetFrame();
  if (!frame)
    return {};

  const FrameSelection& frame_selection = frame->Selection();
  if (!frame_selection.IsAvailable())
    return {};

  return FromSelection(frame_selection.GetSelectionInDOMTree(),
                       selection_behavior);
}

// static
AXSelection AXSelection::FromCurrentSelection(
    const TextControlElement& text_control) {
  const Document& document = text_control.GetDocument();
  AXObjectCache* ax_object_cache = document.ExistingAXObjectCache();
  if (!ax_object_cache)
    return {};

  auto* ax_object_cache_impl = static_cast<AXObjectCacheImpl*>(ax_object_cache);
  const AXObject* ax_text_control = ax_object_cache_impl->Get(&text_control);
  DCHECK(ax_text_control);

  // We can't directly use "text_control.Selection()" because the selection it
  // returns is inside the shadow DOM and it's not anchored to the text field
  // itself.
  const TextAffinity focus_affinity = text_control.Selection().Affinity();
  const TextAffinity anchor_affinity =
      text_control.selectionStart() == text_control.selectionEnd()
          ? focus_affinity
          : TextAffinity::kDownstream;

  const bool is_backward = (text_control.selectionDirection() == "backward");
  const auto ax_anchor = AXPosition::CreatePositionInTextObject(
      *ax_text_control,
      static_cast<int>(is_backward ? text_control.selectionEnd()
                                   : text_control.selectionStart()),
      anchor_affinity);
  const auto ax_focus = AXPosition::CreatePositionInTextObject(
      *ax_text_control,
      static_cast<int>(is_backward ? text_control.selectionStart()
                                   : text_control.selectionEnd()),
      focus_affinity);

  if (!ax_anchor.IsValid() || !ax_focus.IsValid()) {
    return {};
  }

  AXSelection::Builder selection_builder;
  selection_builder.SetAnchor(ax_anchor).SetFocus(ax_focus);
  return selection_builder.Build();
}

// static
AXSelection AXSelection::FromSelection(
    const SelectionInDOMTree& selection,
    const AXSelectionBehavior selection_behavior) {
  if (selection.IsNone())
    return {};
  DCHECK(selection.AssertValid());

  const Position dom_anchor = selection.Anchor();
  const Position dom_focus = selection.Focus();
  const TextAffinity focus_affinity = selection.Affinity();
  const TextAffinity anchor_affinity =
      selection.IsCaret() ? focus_affinity : TextAffinity::kDownstream;

  AXPositionAdjustmentBehavior anchor_adjustment =
      AXPositionAdjustmentBehavior::kMoveRight;
  AXPositionAdjustmentBehavior focus_adjustment =
      AXPositionAdjustmentBehavior::kMoveRight;
  // If the selection is not collapsed, extend or shrink the DOM selection if
  // there is no equivalent selection in the accessibility tree, i.e. if the
  // corresponding endpoints are either ignored or unavailable in the
  // accessibility tree. If the selection is collapsed, move both endpoints to
  // the next valid position in the accessibility tree but do not extend or
  // shrink the selection, because this will result in a non-collapsed selection
  // in the accessibility tree.
  if (!selection.IsCaret()) {
    switch (selection_behavior) {
      case AXSelectionBehavior::kShrinkToValidRange:
        if (selection.IsAnchorFirst()) {
          anchor_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
          focus_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        } else {
          anchor_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
          focus_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        }
        break;
      case AXSelectionBehavior::kExtendToValidRange:
        if (selection.IsAnchorFirst()) {
          anchor_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
          focus_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        } else {
          anchor_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
          focus_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        }
        break;
    }
  }

  const auto ax_anchor =
      AXPosition::FromPosition(dom_anchor, anchor_affinity, anchor_adjustment);
  const auto ax_focus =
      AXPosition::FromPosition(dom_focus, focus_affinity, focus_adjustment);

  if (!ax_anchor.IsValid() || !ax_focus.IsValid()) {
    return {};
  }

  AXSelection::Builder selection_builder;
  selection_builder.SetAnchor(ax_anchor).SetFocus(ax_focus);
  return selection_builder.Build();
}

AXSelection::AXSelection() : anchor_(), focus_() {
#if DCHECK_IS_ON()
  dom_tree_version_ = 0;
  style_version_ = 0;
#endif
}

bool AXSelection::IsValid() const {
  if (!anchor_.IsValid() || !focus_.IsValid()) {
    return false;
  }

  // We don't support selections that span across documents.
  if (anchor_.ContainerObject()->GetDocument() !=
      focus_.ContainerObject()->GetDocument()) {
    return false;
  }

  //
  // The following code checks if a text position in a text control is valid.
  // Since the contents of a text control are implemented using user agent
  // shadow DOM, we want to prevent users from selecting across the shadow DOM
  // boundary.
  //
  // TODO(nektar): Generalize this logic to adjust user selection if it crosses
  // disallowed shadow DOM boundaries such as user agent shadow DOM, editing
  // boundaries, replaced elements, CSS user-select, etc.
  //

  if (anchor_.IsTextPosition() &&
      anchor_.ContainerObject()->IsAtomicTextField() &&
      !(anchor_.ContainerObject() == focus_.ContainerObject() &&
        focus_.IsTextPosition() &&
        focus_.ContainerObject()->IsAtomicTextField())) {
    return false;
  }

  if (focus_.IsTextPosition() &&
      focus_.ContainerObject()->IsAtomicTextField() &&
      !(anchor_.ContainerObject() == focus_.ContainerObject() &&
        anchor_.IsTextPosition() &&
        anchor_.ContainerObject()->IsAtomicTextField())) {
    return false;
  }

  DCHECK(!anchor_.ContainerObject()->GetDocument()->NeedsLayoutTreeUpdate());
#if DCHECK_IS_ON()
  DCHECK_EQ(anchor_.ContainerObject()->GetDocument()->DomTreeVersion(),
            dom_tree_version_);
  DCHECK_EQ(anchor_.ContainerObject()->GetDocument()->StyleVersion(),
            style_version_);
#endif  // DCHECK_IS_ON()
  return true;
}

const SelectionInDOMTree AXSelection::AsSelection(
    const AXSelectionBehavior selection_behavior) const {
  if (!IsValid())
    return {};

  AXPositionAdjustmentBehavior anchor_adjustment =
      AXPositionAdjustmentBehavior::kMoveLeft;
  AXPositionAdjustmentBehavior focus_adjustment =
      AXPositionAdjustmentBehavior::kMoveLeft;
  switch (selection_behavior) {
    case AXSelectionBehavior::kShrinkToValidRange:
      if (anchor_ < focus_) {
        anchor_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        focus_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
      } else if (anchor_ > focus_) {
        anchor_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        focus_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
      }
      break;
    case AXSelectionBehavior::kExtendToValidRange:
      if (anchor_ < focus_) {
        anchor_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
        focus_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
      } else if (anchor_ > focus_) {
        anchor_adjustment = AXPositionAdjustmentBehavior::kMoveRight;
        focus_adjustment = AXPositionAdjustmentBehavior::kMoveLeft;
      }
      break;
  }

  const auto dom_anchor = anchor_.ToPositionWithAffinity(anchor_adjustment);
  const auto dom_focus = focus_.ToPositionWithAffinity(focus_adjustment);
  SelectionInDOMTree::Builder selection_builder;
  selection_builder.SetBaseAndExtent(dom_anchor.GetPosition(),
                                     dom_focus.GetPosition());
  if (focus_.IsTextPosition()) {
    selection_builder.SetAffinity(focus_.Affinity());
  }
  return selection_builder.Build();
}

void AXSelection::UpdateSelectionIfNecessary() {
  Document* document = anchor_.ContainerObject()->GetDocument();
  if (!document)
    return;

  LocalFrameView* view = document->View();
  if (!view || !view->LayoutPending())
    return;

  document->UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
#if DCHECK_IS_ON()
  anchor_.dom_tree_version_ = focus_.dom_tree_version_ = dom_tree_version_ =
      document->DomTreeVersion();
  anchor_.style_version_ = focus_.style_version_ = style_version_ =
      document->StyleVersion();
#endif  // DCHECK_IS_ON()
}

bool AXSelection::Select(const AXSelectionBehavior selection_behavior) {
  if (!IsValid()) {
    // By the time the selection action gets here, content could have
    // changed from the content the action was initially prepared for.
    return false;
  }

  std::optional<AXSelection::TextControlSelection> text_control_selection =
      AsTextControlSelection();

  // We need to make sure we only go into here if we're dealing with a position
  // in the atomic text field. This is because the offsets are being assumed
  // to be on the atomic text field, and not on the descendant inline text
  // boxes.
  if (text_control_selection.has_value() &&
      *anchor_.ContainerObject() ==
          *anchor_.ContainerObject()->GetAtomicTextFieldAncestor() &&
      *focus_.ContainerObject() ==
          *focus_.ContainerObject()->GetAtomicTextFieldAncestor()) {
    DCHECK_LE(text_control_selection->start, text_control_selection->end);
    TextControlElement& text_control = ToTextControl(
        *anchor_.ContainerObject()->GetAtomicTextFieldAncestor()->GetNode());
    if (!text_control.SetSelectionRange(text_control_selection->start,
                                        text_control_selection->end,
                                        text_control_selection->direction)) {
      return false;
    }

    // TextControl::SetSelectionRange deliberately does not set focus. But if
    // we're updating the selection, the text control should be focused.
    ScheduleSelectEvent(text_control);
    text_control.Focus(FocusParams(FocusTrigger::kUserGesture));
    return true;
  }

  const SelectionInDOMTree old_selection = AsSelection(selection_behavior);
  DCHECK(old_selection.AssertValid());
  Document* document = old_selection.Anchor().GetDocument();
  if (!document) {
    // By the time the selection action gets here, content could have
    // changed from the content the action was initially prepared for.
    return false;
  }

  LocalFrame* frame = document->GetFrame();
  if (!frame) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  FrameSelection& frame_selection = frame->Selection();
  if (!frame_selection.IsAvailable())
    return false;

  // See the following section in the Selection API Specification:
  // https://w3c.github.io/selection-api/#selectstart-event
  if (DispatchSelectStart(old_selection.Anchor().ComputeContainerNode()) !=
      DispatchEventResult::kNotCanceled) {
    return false;
  }

  UpdateSelectionIfNecessary();
  if (!IsValid())
    return false;

  // Dispatching the "selectstart" event could potentially change the document
  // associated with the current frame.
  if (!frame_selection.IsAvailable())
    return false;

  // Re-retrieve the SelectionInDOMTree in case a DOM mutation took place.
  // That way it will also have the updated DOM tree and Style versions,
  // and the SelectionTemplate checks for each won't fail.
  const SelectionInDOMTree selection = AsSelection(selection_behavior);

  SetSelectionOptions::Builder options_builder;
  options_builder.SetIsDirectional(true)
      .SetShouldCloseTyping(true)
      .SetShouldClearTypingStyle(true)
      .SetSetSelectionBy(SetSelectionBy::kUser);
  frame_selection.SetSelectionForAccessibility(selection,
                                               options_builder.Build());
  return true;
}

String AXSelection::ToString() const {
  String prefix = IsValid() ? "" : "Invalid ";
  return prefix + "AXSelection from " + Anchor().ToString() + " to " +
         Focus().ToString();
}

std::optional<AXSelection::TextControlSelection>
AXSelection::AsTextControlSelection() const {
  if (!IsValid() || !anchor_.IsTextPosition() || !focus_.IsTextPosition() ||
      anchor_.ContainerObject() != focus_.ContainerObject()) {
    return {};
  }

  const AXObject* text_control =
      anchor_.ContainerObject()->GetAtomicTextFieldAncestor();
  if (!text_control)
    return {};

  DCHECK(IsTextControl(text_control->GetNode()));

  if (anchor_ <= focus_) {
    return TextControlSelection(anchor_.TextOffset(), focus_.TextOffset(),
                                kSelectionHasForwardDirection);
  }
  return TextControlSelection(focus_.TextOffset(), anchor_.TextOffset(),
                              kSelectionHasBackwardDirection);
}

bool operator==(const AXSelection& a, const AXSelection& b) {
  DCHECK(a.IsValid() && b.IsValid());
  return a.Anchor() == b.Anchor() && a.Focus() == b.Focus();
}

bool operator!=(const AXSelection& a, const AXSelection& b) {
  return !(a == b);
}

std::ostream& operator<<(std::ostream& ostream, const AXSelection& selection) {
  return ostream << selection.ToString().Utf8();
}

}  // namespace blink
