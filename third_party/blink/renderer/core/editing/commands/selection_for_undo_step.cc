// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/selection_for_undo_step.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"

namespace blink {

SelectionForUndoStep SelectionForUndoStep::From(
    const SelectionInDOMTree& selection) {
  SelectionForUndoStep result;
  result.anchor_ = selection.Anchor();
  result.focus_ = selection.Focus();
  result.affinity_ = selection.Affinity();
  result.is_anchor_first_ = selection.IsAnchorFirst();
  result.root_editable_element_ = RootEditableElementOf(result.anchor_);
  return result;
}

SelectionForUndoStep::SelectionForUndoStep(const SelectionForUndoStep& other) =
    default;

SelectionForUndoStep::SelectionForUndoStep() = default;

SelectionForUndoStep& SelectionForUndoStep::operator=(
    const SelectionForUndoStep& other) = default;

bool SelectionForUndoStep::operator==(const SelectionForUndoStep& other) const {
  if (IsNone()) {
    return other.IsNone();
  }
  if (other.IsNone()) {
    return false;
  }
  return anchor_ == other.anchor_ && focus_ == other.focus_ &&
         affinity_ == other.affinity_ &&
         is_anchor_first_ == other.is_anchor_first_;
}

bool SelectionForUndoStep::operator!=(const SelectionForUndoStep& other) const {
  return !operator==(other);
}

SelectionInDOMTree SelectionForUndoStep::AsSelection() const {
  if (IsNone()) {
    return SelectionInDOMTree();
  }
  return SelectionInDOMTree::Builder()
      .SetBaseAndExtent(anchor_, focus_)
      .SetAffinity(affinity_)
      .Build();
}

Position SelectionForUndoStep::Start() const {
  return is_anchor_first_ ? anchor_ : focus_;
}

Position SelectionForUndoStep::End() const {
  return is_anchor_first_ ? focus_ : anchor_;
}

bool SelectionForUndoStep::IsCaret() const {
  return anchor_.IsNotNull() && anchor_ == focus_;
}

bool SelectionForUndoStep::IsNone() const {
  return anchor_.IsNull();
}

bool SelectionForUndoStep::IsRange() const {
  return anchor_ != focus_;
}

bool SelectionForUndoStep::IsValidFor(const Document& document) const {
  if (anchor_.IsNull()) {
    return true;
  }
  return anchor_.IsValidFor(document) && focus_.IsValidFor(document);
}

void SelectionForUndoStep::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_);
  visitor->Trace(focus_);
  visitor->Trace(root_editable_element_);
}

// ---
SelectionForUndoStep::Builder::Builder() = default;

SelectionForUndoStep::Builder&
SelectionForUndoStep::Builder::SetAnchorAndFocusAsBackwardSelection(
    const Position& anchor,
    const Position& focus) {
  DCHECK(anchor.IsNotNull());
  DCHECK(focus.IsNotNull());
  DCHECK_NE(anchor, focus);
  selection_.anchor_ = anchor;
  selection_.focus_ = focus;
  selection_.is_anchor_first_ = false;
  return *this;
}

SelectionForUndoStep::Builder&
SelectionForUndoStep::Builder::SetAnchorAndFocusAsForwardSelection(
    const Position& anchor,
    const Position& focus) {
  DCHECK(anchor.IsNotNull());
  DCHECK(focus.IsNotNull());
  DCHECK_NE(anchor, focus);
  selection_.anchor_ = anchor;
  selection_.focus_ = focus;
  selection_.is_anchor_first_ = true;
  return *this;
}

void SelectionForUndoStep::Builder::Trace(Visitor* visitor) const {
  visitor->Trace(selection_);
}

// ---
VisibleSelection CreateVisibleSelection(
    const SelectionForUndoStep& selection_in_undo_step) {
  return CreateVisibleSelection(selection_in_undo_step.AsSelection());
}

}  // namespace blink
