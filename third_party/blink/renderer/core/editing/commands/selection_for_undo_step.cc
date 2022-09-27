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
  result.base_ = selection.Base();
  result.extent_ = selection.Extent();
  result.affinity_ = selection.Affinity();
  result.is_base_first_ = selection.IsBaseFirst();
  result.root_editable_element_ = RootEditableElementOf(result.base_);
  return result;
}

SelectionForUndoStep::SelectionForUndoStep(const SelectionForUndoStep& other) =
    default;

SelectionForUndoStep::SelectionForUndoStep() = default;

SelectionForUndoStep& SelectionForUndoStep::operator=(
    const SelectionForUndoStep& other) = default;

bool SelectionForUndoStep::operator==(const SelectionForUndoStep& other) const {
  if (IsNone())
    return other.IsNone();
  if (other.IsNone())
    return false;
  return base_ == other.base_ && extent_ == other.extent_ &&
         affinity_ == other.affinity_ && is_base_first_ == other.is_base_first_;
}

bool SelectionForUndoStep::operator!=(const SelectionForUndoStep& other) const {
  return !operator==(other);
}

SelectionInDOMTree SelectionForUndoStep::AsSelection() const {
  if (IsNone()) {
    return SelectionInDOMTree();
  }
  return SelectionInDOMTree::Builder()
      .SetBaseAndExtent(base_, extent_)
      .SetAffinity(affinity_)
      .Build();
}

Position SelectionForUndoStep::Start() const {
  return is_base_first_ ? base_ : extent_;
}

Position SelectionForUndoStep::End() const {
  return is_base_first_ ? extent_ : base_;
}

bool SelectionForUndoStep::IsCaret() const {
  return base_.IsNotNull() && base_ == extent_;
}

bool SelectionForUndoStep::IsNone() const {
  return base_.IsNull();
}

bool SelectionForUndoStep::IsRange() const {
  return base_ != extent_;
}

bool SelectionForUndoStep::IsValidFor(const Document& document) const {
  if (base_.IsNull())
    return true;
  return base_.IsValidFor(document) && extent_.IsValidFor(document);
}

void SelectionForUndoStep::Trace(Visitor* visitor) const {
  visitor->Trace(base_);
  visitor->Trace(extent_);
  visitor->Trace(root_editable_element_);
}

// ---
SelectionForUndoStep::Builder::Builder() = default;

SelectionForUndoStep::Builder&
SelectionForUndoStep::Builder::SetBaseAndExtentAsBackwardSelection(
    const Position& base,
    const Position& extent) {
  DCHECK(base.IsNotNull());
  DCHECK(extent.IsNotNull());
  DCHECK_NE(base, extent);
  selection_.base_ = base;
  selection_.extent_ = extent;
  selection_.is_base_first_ = false;
  return *this;
}

SelectionForUndoStep::Builder&
SelectionForUndoStep::Builder::SetBaseAndExtentAsForwardSelection(
    const Position& base,
    const Position& extent) {
  DCHECK(base.IsNotNull());
  DCHECK(extent.IsNotNull());
  DCHECK_NE(base, extent);
  selection_.base_ = base;
  selection_.extent_ = extent;
  selection_.is_base_first_ = true;
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
