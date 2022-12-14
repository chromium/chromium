// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include <cstddef>
#include <iterator>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/web_ax_platform_tree_manager_delegate.h"
#include "content/common/ax_serialization_utils.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/strings/grit/blink_accessibility_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/base/buildflags.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/strings/grit/ax_strings.h"

namespace content {

#if !BUILDFLAG(HAS_PLATFORM_ACCESSIBILITY_SUPPORT)
// static
std::unique_ptr<BrowserAccessibility> BrowserAccessibility::Create(
    BrowserAccessibilityManager* manager,
    ui::AXNode* node) {
  return std::unique_ptr<BrowserAccessibility>(
      new BrowserAccessibility(manager, node));
}
#endif  // !BUILDFLAG(HAS_PLATFORM_ACCESSIBILITY_SUPPORT)

// static
BrowserAccessibility* BrowserAccessibility::FromAXPlatformNodeDelegate(
    ui::AXPlatformNodeDelegate* delegate) {
  if (!delegate || !delegate->IsWebContent())
    return nullptr;
  return static_cast<BrowserAccessibility*>(delegate);
}

BrowserAccessibility::BrowserAccessibility(BrowserAccessibilityManager* manager,
                                           ui::AXNode* node)
    : AXPlatformNodeDelegate(node), manager_(manager) {
  DCHECK(manager);
  DCHECK(node);
  DCHECK(node->IsDataValid());
}

BrowserAccessibility::~BrowserAccessibility() = default;

namespace {

// Get the native text field's deepest container; the lowest descendant that
// contains all its text. Returns nullptr if the text field is empty, or if it
// is not a native text field (input or textarea).
BrowserAccessibility* GetTextFieldInnerEditorElement(
    const BrowserAccessibility& text_field) {
  ui::AXNode* text_container =
      text_field.node()->GetTextFieldInnerEditorElement();
  return text_field.manager()->GetFromAXNode(text_container);
}

}  // namespace

bool BrowserAccessibility::IsValid() const {
  // Currently we only perform validity checks on non-empty, atomic text fields.
  // An atomic text field does not expose its internal implementation to
  // assistive software, appearing as a single leaf node in the accessibility
  // tree. It includes <input>, <textarea> and Views-based text fields.
  if (!IsAtomicTextField())
    return true;

  // If the input type is not plain or text it may be a complex field, such as
  // a datetime input. We don't try to enforce a special structure for those.
  const std::string& input_type =
      GetStringAttribute(ax::mojom::StringAttribute::kInputType);
  DCHECK(IsIgnored() ||
         GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag) != "input" ||
         !input_type.empty())
      << "By design, all non-hidden <input> elements in the accessibility "
         "tree, should have an input type: "
      << *this;
  if (input_type != "text" && input_type != "search")
    return true;  // Not a plain text field, just consider it valid.

  if (InternalChildCount()) {
    // If the atomic text field is aria-hidden then all its descendants are
    // ignored.
    //   See the dump tree test AccessibilityAriaHiddenFocusedInput.
    //
    // TODO(accessibility): We need to fix this by pruning the tree and removing
    // the native text field if it is aria-hidden.
    return IsInvisibleOrIgnored() || GetTextFieldInnerEditorElement(*this);
  }
  return true;
}

void BrowserAccessibility::OnDataChanged() {
  DCHECK(IsValid()) << "Invalid node: " << *this;
}

bool BrowserAccessibility::CanFireEvents() const {
  return node()->CanFireEvents();
}

ui::AXPlatformNode* BrowserAccessibility::GetAXPlatformNode() const {
  // Not all BrowserAccessibility subclasses can return an AXPlatformNode yet.
  // So, here we just return nullptr.
  return nullptr;
}

size_t BrowserAccessibility::PlatformChildCount() const {
  // We need to explicitly check for leafiness here instead of relying on
  // `AXNode::IsLeaf()` because Android has a different notion of this concept.
  return IsLeaf() ? 0 : node()->GetUnignoredChildCountCrossingTreeBoundary();
}

BrowserAccessibility* BrowserAccessibility::PlatformGetParent() const {
  ui::AXNode* parent = node()->GetUnignoredParentCrossingTreeBoundary();
  return manager()->GetFromAXNode(parent);
}

BrowserAccessibility* BrowserAccessibility::PlatformGetFirstChild() const {
  // We need to explicitly check for leafiness here instead of relying on
  // `AXNode::IsLeaf()` because Android has a different notion of this concept.
  if (IsLeaf())
    return nullptr;
  ui::AXNode* first_child =
      node()->GetFirstUnignoredChildCrossingTreeBoundary();
  return manager()->GetFromAXNode(first_child);
}

BrowserAccessibility* BrowserAccessibility::PlatformGetLastChild() const {
  // We need to explicitly check for leafiness here instead of relying on
  // `AXNode::IsLeaf()` because Android has a different notion of this concept.
  if (IsLeaf())
    return nullptr;
  ui::AXNode* last_child = node()->GetLastUnignoredChildCrossingTreeBoundary();
  return manager()->GetFromAXNode(last_child);
}

BrowserAccessibility* BrowserAccessibility::PlatformGetNextSibling() const {
  return InternalGetNextSibling();
}

BrowserAccessibility* BrowserAccessibility::PlatformGetPreviousSibling() const {
  return InternalGetPreviousSibling();
}

BrowserAccessibility::PlatformChildIterator
BrowserAccessibility::PlatformChildrenBegin() const {
  return PlatformChildIterator(this, PlatformGetFirstChild());
}

BrowserAccessibility::PlatformChildIterator
BrowserAccessibility::PlatformChildrenEnd() const {
  return PlatformChildIterator(this, nullptr);
}

bool BrowserAccessibility::IsDescendantOf(
    const BrowserAccessibility* ancestor) const {
  if (!ancestor)
    return false;
  DCHECK(ancestor->node());
  return node()->IsDescendantOfCrossingTreeBoundary(ancestor->node());
}

bool BrowserAccessibility::IsIgnoredForTextNavigation() const {
  return node()->IsIgnoredForTextNavigation();
}

bool BrowserAccessibility::IsLineBreakObject() const {
  return node()->IsLineBreak();
}

BrowserAccessibility* BrowserAccessibility::PlatformGetChild(
    size_t child_index) const {
  // We need to explicitly check for leafiness here instead of relying on
  // `AXNode::IsLeaf()` because Android has a different notion of this concept.
  if (IsLeaf())
    return nullptr;
  ui::AXNode* child =
      node()->GetUnignoredChildAtIndexCrossingTreeBoundary(child_index);
  return manager()->GetFromAXNode(child);
}

BrowserAccessibility* BrowserAccessibility::PlatformGetLowestPlatformAncestor()
    const {
  ui::AXNode* lowest_platform_ancestor = node()->GetLowestPlatformAncestor();
  return manager()->GetFromAXNode(lowest_platform_ancestor);
}

BrowserAccessibility* BrowserAccessibility::PlatformGetTextFieldAncestor()
    const {
  ui::AXNode* text_field_ancestor = node()->GetTextFieldAncestor();
  return manager()->GetFromAXNode(text_field_ancestor);
}

BrowserAccessibility* BrowserAccessibility::PlatformGetSelectionContainer()
    const {
  ui::AXNode* selection_container_ancestor = node()->GetSelectionContainer();
  return manager()->GetFromAXNode(selection_container_ancestor);
}

BrowserAccessibility* BrowserAccessibility::PlatformDeepestFirstChild() const {
  // We need to explicitly check for leafiness here instead of relying on
  // `AXNode::IsLeaf()` because Android has a different notion of this concept.
  if (IsLeaf())
    return nullptr;
  BrowserAccessibility* deepest_child = PlatformGetFirstChild();
  while (!deepest_child->IsLeaf())
    deepest_child = deepest_child->PlatformGetFirstChild();
  return deepest_child;
}

BrowserAccessibility* BrowserAccessibility::PlatformDeepestLastChild() const {
  // We need to explicitly check for leafiness here instead of relying on
  // `AXNode::IsLeaf()` because Android has a different notion of this concept.
  if (IsLeaf())
    return nullptr;
  BrowserAccessibility* deepest_child = PlatformGetLastChild();
  while (!deepest_child->IsLeaf())
    deepest_child = deepest_child->PlatformGetLastChild();
  return deepest_child;
}

BrowserAccessibility* BrowserAccessibility::InternalDeepestFirstChild() const {
  // By design, this method should be able to traverse platform leaves, hence we
  // don't check for leafiness.
  ui::AXNode* deepest_child = node()->GetDeepestFirstUnignoredChild();
  return manager()->GetFromAXNode(deepest_child);
}

BrowserAccessibility* BrowserAccessibility::InternalDeepestLastChild() const {
  // By design, this method should be able to traverse platform leaves, hence we
  // don't check for leafiness. We need to explicitly check for leafiness here
  // instead of relying on `AXNode::IsLeaf()` because Android has a different
  // notion of this concept.
  ui::AXNode* deepest_child = node()->GetDeepestLastUnignoredChild();
  return manager()->GetFromAXNode(deepest_child);
}

size_t BrowserAccessibility::InternalChildCount() const {
  return node()->GetUnignoredChildCount();
}

BrowserAccessibility* BrowserAccessibility::InternalGetChild(
    size_t child_index) const {
  // By design, this method should be able to traverse platform leaves, hence we
  // don't check for leafiness.
  ui::AXNode* child_node = node()->GetUnignoredChildAtIndex(child_index);
  return manager_->GetFromAXNode(child_node);
}

BrowserAccessibility* BrowserAccessibility::InternalGetParent() const {
  ui::AXNode* parent_node = node()->GetUnignoredParent();
  return manager_->GetFromAXNode(parent_node);
}

BrowserAccessibility* BrowserAccessibility::InternalGetFirstChild() const {
  // By design, this method should be able to traverse platform leaves, hence we
  // don't check for leafiness.
  ui::AXNode* child_node = node()->GetFirstUnignoredChild();
  return manager_->GetFromAXNode(child_node);
}

BrowserAccessibility* BrowserAccessibility::InternalGetLastChild() const {
  // By design, this method should be able to traverse platform leaves, hence we
  // don't check for leafiness.
  ui::AXNode* child_node = node()->GetLastUnignoredChild();
  return manager_->GetFromAXNode(child_node);
}

BrowserAccessibility* BrowserAccessibility::InternalGetNextSibling() const {
  ui::AXNode* sibling_node = node()->GetNextUnignoredSibling();
  return manager_->GetFromAXNode(sibling_node);
}

BrowserAccessibility* BrowserAccessibility::InternalGetPreviousSibling() const {
  ui::AXNode* sibling_node = node()->GetPreviousUnignoredSibling();
  return manager_->GetFromAXNode(sibling_node);
}

BrowserAccessibility::InternalChildIterator
BrowserAccessibility::InternalChildrenBegin() const {
  return InternalChildIterator(this, InternalGetFirstChild());
}

BrowserAccessibility::InternalChildIterator
BrowserAccessibility::InternalChildrenEnd() const {
  return InternalChildIterator(this, nullptr);
}

const BrowserAccessibility*
BrowserAccessibility::AllChildrenRange::Iterator::operator*() {
  if (child_tree_root_)
    return index_ == 0 ? child_tree_root_ : nullptr;

  // TODO(nektar): Consider using
  // `AXNode::GetChildAtIndexCrossingTreeBoundary()`.
  ui::AXNode* child = parent_->node()->GetChildAtIndex(index_);
  return parent_->manager()->GetFromAXNode(child);
}

gfx::RectF BrowserAccessibility::GetLocation() const {
  return GetData().relative_bounds.bounds;
}

gfx::Rect BrowserAccessibility::GetUnclippedRootFrameHypertextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    ui::AXOffscreenResult* offscreen_result) const {
  return GetHypertextRangeBoundsRect(
      start_offset, end_offset, ui::AXCoordinateSystem::kRootFrame,
      ui::AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetUnclippedScreenInnerTextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    ui::AXOffscreenResult* offscreen_result) const {
  return GetInnerTextRangeBoundsRect(
      start_offset, end_offset, ui::AXCoordinateSystem::kScreenDIPs,
      ui::AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetUnclippedRootFrameInnerTextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    ui::AXOffscreenResult* offscreen_result) const {
  return GetInnerTextRangeBoundsRect(
      start_offset, end_offset, ui::AXCoordinateSystem::kRootFrame,
      ui::AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetBoundsRect(
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  return RelativeToAbsoluteBounds(gfx::RectF(), coordinate_system,
                                  clipping_behavior, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetHypertextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  int effective_start_offset = start_offset;
  int effective_end_offset = end_offset;

  if (effective_start_offset == effective_end_offset)
    return gfx::Rect();
  if (effective_start_offset > effective_end_offset)
    std::swap(effective_start_offset, effective_end_offset);

  const std::u16string& text_str = GetHypertext();
  if (effective_start_offset < 0 ||
      effective_start_offset >= static_cast<int>(text_str.size())) {
    return gfx::Rect();
  }
  if (effective_end_offset < 0 ||
      effective_end_offset > static_cast<int>(text_str.size())) {
    return gfx::Rect();
  }

  if (coordinate_system == ui::AXCoordinateSystem::kFrame) {
    NOTIMPLEMENTED();
    return gfx::Rect();
  }

  // Obtain bounds in root frame coordinates.
  gfx::Rect bounds = GetRootFrameHypertextRangeBoundsRect(
      effective_start_offset, effective_end_offset - effective_start_offset,
      clipping_behavior, offscreen_result);

  if (coordinate_system == ui::AXCoordinateSystem::kScreenDIPs ||
      coordinate_system == ui::AXCoordinateSystem::kScreenPhysicalPixels) {
    // Convert to screen coordinates.
    bounds.Offset(
        manager()->GetViewBoundsInScreenCoordinates().OffsetFromOrigin());
  }

  return bounds;
}

gfx::Rect BrowserAccessibility::GetRootFrameHypertextRangeBoundsRect(
    int start,
    int len,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  // TODO(nektar): Move to `AXNode` as soon as hypertext computation is fully
  // migrated to that class.
  DCHECK_GE(start, 0);
  DCHECK_GE(len, 0);

  // Atomic text fields such as textarea have a text container node inside them
  // that holds all the text and do not expose any IA2 hypertext. We need to get
  // to the flattened representation of the text in the field in order that
  // `start` and `len` would be applicable. Non-native text fields, including
  // ARIA-based ones expose their actual subtree and do use IA2 hypertext, so
  // `start` and `len` would apply in those cases.
  if (const BrowserAccessibility* text_container =
          GetTextFieldInnerEditorElement(*this)) {
    return text_container->GetRootFrameHypertextRangeBoundsRect(
        start, len, clipping_behavior, offscreen_result);
  }

  if (GetRole() != ax::mojom::Role::kStaticText) {
    gfx::Rect bounds;
    for (InternalChildIterator it = InternalChildrenBegin();
         it != InternalChildrenEnd() && len > 0; ++it) {
      const BrowserAccessibility* child = it.get();
      // Child objects are of length one, since they are represented by a single
      // embedded object character. The exception is text-only objects.
      int child_length_in_parent = 1;
      if (child->IsText())
        child_length_in_parent = static_cast<int>(child->GetHypertext().size());
      if (start < child_length_in_parent) {
        gfx::Rect child_rect;
        if (child->IsText()) {
          child_rect = child->GetRootFrameHypertextRangeBoundsRect(
              start, len, clipping_behavior, offscreen_result);
        } else {
          child_rect = child->GetRootFrameHypertextRangeBoundsRect(
              0, static_cast<int>(child->GetHypertext().size()),
              clipping_behavior, offscreen_result);
        }
        bounds.Union(child_rect);
        len -= (child_length_in_parent - start);
      }
      if (start > child_length_in_parent)
        start -= child_length_in_parent;
      else
        start = 0;
    }
    // When past the end of text, the area will be 0.
    // In this case, use bounds provided for the caret.
    return bounds.IsEmpty() ? GetRootFrameHypertextBoundsPastEndOfText(
                                  clipping_behavior, offscreen_result)
                            : bounds;
  }

  int end = start + len;
  int child_start = 0;
  int child_end = 0;
  gfx::Rect bounds;
  for (InternalChildIterator it = InternalChildrenBegin();
       it != InternalChildrenEnd() && child_end < start + len; ++it) {
    const BrowserAccessibility* child = it.get();
    if (child->GetRole() != ax::mojom::Role::kInlineTextBox) {
      DLOG(WARNING) << "BrowserAccessibility objects with role STATIC_TEXT "
                    << "should have children of role INLINE_TEXT_BOX.\n";
      continue;
    }

    int child_length = static_cast<int>(child->GetHypertext().size());
    child_start = child_end;
    child_end += child_length;

    if (child_end < start)
      continue;

    int overlap_start = std::max(start, child_start);
    int overlap_end = std::min(end, child_end);

    int local_start = overlap_start - child_start;
    int local_end = overlap_end - child_start;
    // |local_end| and |local_start| may equal |child_length| when the caret is
    // at the end of a text field.
    DCHECK_GE(local_start, 0);
    DCHECK_LE(local_start, child_length);
    DCHECK_GE(local_end, 0);
    DCHECK_LE(local_end, child_length);

    // Don't clip bounds. Some screen magnifiers (e.g. ZoomText) prefer to
    // get unclipped bounds so that they can make smooth scrolling calculations.
    gfx::Rect absolute_child_rect = child->RelativeToAbsoluteBounds(
        child->GetTextContentRangeBoundsUTF16(local_start, local_end),
        ui::AXCoordinateSystem::kRootFrame, clipping_behavior,
        offscreen_result);
    if (bounds.width() == 0 && bounds.height() == 0) {
      bounds = absolute_child_rect;
    } else {
      bounds.Union(absolute_child_rect);
    }
  }

  return bounds;
}

gfx::Rect BrowserAccessibility::GetScreenHypertextRangeBoundsRect(
    int start,
    int len,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  gfx::Rect bounds = GetRootFrameHypertextRangeBoundsRect(
      start, len, clipping_behavior, offscreen_result);

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  bounds.Offset(
      manager_->GetViewBoundsInScreenCoordinates().OffsetFromOrigin());

  return bounds;
}

gfx::Rect BrowserAccessibility::GetRootFrameHypertextBoundsPastEndOfText(
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  // TODO(nektar): Move to `AXNode` as soon as hypertext computation is fully
  // migrated to that class.

  // Step 1: get approximate caret bounds. The thickness may not yet be correct.
  gfx::Rect bounds;
  if (InternalChildCount() > 0) {
    // When past the end of text, use bounds provided by a last child if
    // available, and then correct for thickness of caret.
    BrowserAccessibility* child = InternalGetLastChild();
    int child_text_len = child->GetHypertext().size();
    bounds = child->GetRootFrameHypertextRangeBoundsRect(
        child_text_len, child_text_len, clipping_behavior, offscreen_result);
    if (bounds.width() == 0 && bounds.height() == 0)
      return bounds;  // Inline text boxes info not yet available.
  } else {
    // Compute bounds of where caret would be, based on bounds of object.
    bounds = GetBoundsRect(ui::AXCoordinateSystem::kRootFrame,
                           clipping_behavior, offscreen_result);
  }

  // Step 2: correct for the thickness of the caret.
  auto text_direction = static_cast<ax::mojom::WritingDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection));
  constexpr int kCaretThickness = 1;
  switch (text_direction) {
    case ax::mojom::WritingDirection::kNone:
    case ax::mojom::WritingDirection::kLtr: {
      bounds.set_width(kCaretThickness);
      break;
    }
    case ax::mojom::WritingDirection::kRtl: {
      bounds.set_x(bounds.right() - kCaretThickness);
      bounds.set_width(kCaretThickness);
      break;
    }
    case ax::mojom::WritingDirection::kTtb: {
      bounds.set_height(kCaretThickness);
      break;
    }
    case ax::mojom::WritingDirection::kBtt: {
      bounds.set_y(bounds.bottom() - kCaretThickness);
      bounds.set_height(kCaretThickness);
      break;
    }
  }
  return bounds;
}

gfx::Rect BrowserAccessibility::GetInnerTextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  const int text_length = GetTextContentUTF16().length();
  if (start_offset < 0 || end_offset > text_length || start_offset > end_offset)
    return gfx::Rect();

  return GetInnerTextRangeBoundsRectInSubtree(
      start_offset, end_offset, coordinate_system, clipping_behavior,
      offscreen_result);
}

gfx::Rect BrowserAccessibility::GetInnerTextRangeBoundsRectInSubtree(
    const int start_offset,
    const int end_offset,
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  // TODO(nektar): Move to `AXNode` as soon as hypertext computation is fully
  // migrated to that class.
  if (GetRole() == ax::mojom::Role::kInlineTextBox) {
    return RelativeToAbsoluteBounds(
        GetTextContentRangeBoundsUTF16(start_offset, end_offset),
        coordinate_system, clipping_behavior, offscreen_result);
  }

  gfx::Rect bounds;
  int child_offset_in_parent = 0;
  for (InternalChildIterator it = InternalChildrenBegin();
       it != InternalChildrenEnd(); ++it) {
    const BrowserAccessibility* browser_accessibility_child = it.get();
    const int child_text_length =
        browser_accessibility_child->GetTextContentUTF16().length();

    // The text bounds queried are not in this subtree; skip it and continue.
    const int child_start_offset =
        std::max(start_offset - child_offset_in_parent, 0);
    if (child_start_offset > child_text_length) {
      child_offset_in_parent += child_text_length;
      continue;
    }

    // The text bounds queried have already been gathered; short circuit.
    const int child_end_offset =
        std::min(end_offset - child_offset_in_parent, child_text_length);
    if (child_end_offset < 0)
      return bounds;

    // Increase the text bounds by the subtree text bounds.
    const gfx::Rect child_bounds =
        browser_accessibility_child->GetInnerTextRangeBoundsRectInSubtree(
            child_start_offset, child_end_offset, coordinate_system,
            clipping_behavior, offscreen_result);
    if (bounds.IsEmpty())
      bounds = child_bounds;
    else
      bounds.Union(child_bounds);

    child_offset_in_parent += child_text_length;
  }

  return bounds;
}

gfx::RectF BrowserAccessibility::GetTextContentRangeBoundsUTF16(
    int start_offset,
    int end_offset) const {
  return node()->GetTextContentRangeBoundsUTF16(start_offset, end_offset);
}

BrowserAccessibility* BrowserAccessibility::ApproximateHitTest(
    const gfx::Point& blink_screen_point) {
  // TODO(crbug.com/1049261): This is one of the few methods that won't be moved
  // to `AXNode` in the foreseeable future because the functionality it provides
  // is not immediately needed in Views.

  // The best result found that's a child of this object.
  BrowserAccessibility* child_result = nullptr;
  // The best result that's an indirect descendant like grandchild, etc.
  BrowserAccessibility* descendant_result = nullptr;

  // Walk the children recursively looking for the BrowserAccessibility that
  // most tightly encloses the specified point. Walk backwards so that in
  // the absence of any other information, we assume the object that occurs
  // later in the tree is on top of one that comes before it.
  for (BrowserAccessibility* child = PlatformGetLastChild(); child != nullptr;
       child = child->PlatformGetPreviousSibling()) {
    // Skip table columns because cells are only contained in rows,
    // not columns.
    if (child->GetRole() == ax::mojom::Role::kColumn)
      continue;

    if (child->GetClippedScreenBoundsRect().Contains(blink_screen_point)) {
      BrowserAccessibility* result =
          child->ApproximateHitTest(blink_screen_point);
      if (result == child && !child_result)
        child_result = result;
      if (result != child && !descendant_result)
        descendant_result = result;
    }

    if (child_result && descendant_result)
      break;
  }

  // Explanation of logic: it's possible that this point overlaps more than
  // one child of this object. If so, as a heuristic we prefer if the point
  // overlaps a descendant of one of the two children and not the other.
  // As an example, suppose you have two rows of buttons - the buttons don't
  // overlap, but the rows do. Without this heuristic, we'd greedily only
  // consider one of the containers.
  if (descendant_result)
    return descendant_result;
  if (child_result)
    return child_result;

  return this;
}

bool BrowserAccessibility::IsRootWebAreaForPresentationalIframe() const {
  return node()->IsRootWebAreaForPresentationalIframe();
}

bool BrowserAccessibility::IsClickable() const {
  return GetData().IsClickable();
}

bool BrowserAccessibility::IsTextField() const {
  return GetData().IsTextField();
}

bool BrowserAccessibility::IsPasswordField() const {
  return GetData().IsPasswordField();
}

bool BrowserAccessibility::IsAtomicTextField() const {
  return GetData().IsAtomicTextField();
}

bool BrowserAccessibility::IsNonAtomicTextField() const {
  return GetData().IsNonAtomicTextField();
}

bool BrowserAccessibility::HasExplicitlyEmptyName() const {
  return GetNameFrom() == ax::mojom::NameFrom::kAttributeExplicitlyEmpty;
}

// |offset| could either be a text character or a child index in case of
// non-text objects.
// Currently, to be safe, we convert to text leaf equivalents and we don't use
// tree positions.
// TODO(nektar): Remove this function once selection fixes in Blink are
// thoroughly tested and convert to tree positions.
BrowserAccessibility::AXPosition
BrowserAccessibility::CreatePositionForSelectionAt(int offset) const {
  return CreateTextPositionAt(offset)->AsDomSelectionPosition();
}

std::u16string BrowserAccessibility::GetNameAsString16() const {
  return node()->GetNameUTF16();
}

gfx::Rect BrowserAccessibility::RelativeToAbsoluteBounds(
    gfx::RectF bounds,
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  // TODO(crbug.com/1049261): This is one of the few methods that won't be moved
  // to `AXNode` in the foreseeable future because the functionality it provides
  // is not immediately needed in Views.

  const bool clip_bounds =
      clipping_behavior == ui::AXClippingBehavior::kClipped;
  bool offscreen = false;
  const BrowserAccessibility* node = this;
  while (node) {
    BrowserAccessibilityManager* manager = node->manager();
    bounds = manager->ax_tree()->RelativeToTreeBounds(node->node(), bounds,
                                                      &offscreen, clip_bounds);

    // On some platforms we need to unapply root scroll offsets.
    if (!manager->UseRootScrollOffsetsWhenComputingBounds()) {
      // Get the node that's the "root scroller", which isn't necessarily
      // the root of the tree.
      ui::AXNodeID root_scroller_id = manager->GetTreeData().root_scroller_id;
      BrowserAccessibility* root_scroller =
          manager->GetFromID(root_scroller_id);
      if (root_scroller) {
        int sx = 0;
        int sy = 0;
        if (root_scroller->GetIntAttribute(ax::mojom::IntAttribute::kScrollX,
                                           &sx) &&
            root_scroller->GetIntAttribute(ax::mojom::IntAttribute::kScrollY,
                                           &sy)) {
          bounds.Offset(sx, sy);
        }
      }
    }

    if (coordinate_system == ui::AXCoordinateSystem::kFrame)
      break;

    const BrowserAccessibility* root = manager->GetBrowserAccessibilityRoot();
    node = root->PlatformGetParent();
  }

  if (coordinate_system == ui::AXCoordinateSystem::kScreenDIPs ||
      coordinate_system == ui::AXCoordinateSystem::kScreenPhysicalPixels) {
    // Most platforms include page scale factor in the transform on the root
    // node of the AXTree. That transform gets applied by the call to
    // RelativeToTreeBounds() in the loop above. However, if the root transform
    // did not include page scale factor, we need to apply it now.
    // TODO(crbug.com/1074116): this should probably apply visual viewport
    // offset as well.
    if (!content::AXShouldIncludePageScaleFactorInRoot()) {
      BrowserAccessibilityManager* root_manager =
          manager()->GetManagerForRootFrame();
      if (root_manager)
        bounds.Scale(root_manager->GetPageScaleFactor());
    }
    bounds.Offset(
        manager()->GetViewBoundsInScreenCoordinates().OffsetFromOrigin());
  }

  if (offscreen_result) {
    *offscreen_result = offscreen ? ui::AXOffscreenResult::kOffscreen
                                  : ui::AXOffscreenResult::kOnscreen;
  }

  return gfx::ToEnclosingRect(bounds);
}

bool BrowserAccessibility::IsOffscreen() const {
  ui::AXOffscreenResult offscreen_result = ui::AXOffscreenResult::kOnscreen;
  RelativeToAbsoluteBounds(gfx::RectF(), ui::AXCoordinateSystem::kRootFrame,
                           ui::AXClippingBehavior::kClipped, &offscreen_result);
  return offscreen_result == ui::AXOffscreenResult::kOffscreen;
}

bool BrowserAccessibility::IsWebContent() const {
  return true;
}

bool BrowserAccessibility::HasVisibleCaretOrSelection() const {
  // The caret should be visible if Caret Browsing is enabled.
  //
  // TODO(crbug.com/1052091): Caret Browsing should be looking at leaf text
  // nodes so it might not return expected results in this method.
  if (BrowserAccessibilityStateImpl::GetInstance()->IsCaretBrowsingEnabled())
    return true;
  return node()->HasVisibleCaretOrSelection();
}

std::set<ui::AXPlatformNode*> BrowserAccessibility::GetNodesForNodeIdSet(
    const std::set<int32_t>& ids) {
  std::set<ui::AXPlatformNode*> nodes;
  for (int32_t node_id : ids) {
    if (ui::AXPlatformNode* node = GetFromNodeID(node_id)) {
      nodes.insert(node);
    }
  }
  return nodes;
}

ui::AXPlatformNode* BrowserAccessibility::GetTargetNodeForRelation(
    ax::mojom::IntAttribute attr) {
  DCHECK(ui::IsNodeIdIntAttribute(attr));

  int target_id;
  if (!node()->GetIntAttribute(attr, &target_id))
    return nullptr;

  return GetFromNodeID(target_id);
}

std::vector<ui::AXPlatformNode*>
BrowserAccessibility::GetTargetNodesForRelation(
    ax::mojom::IntListAttribute attr) {
  DCHECK(ui::IsNodeIdIntListAttribute(attr));

  std::vector<int32_t> target_ids;
  if (!GetIntListAttribute(attr, &target_ids))
    return std::vector<ui::AXPlatformNode*>();

  // If we use std::set to eliminate duplicates, the resulting set will be
  // sorted by the id and we will lose the original order provided by the
  // author which may be of interest to ATs. The number of ids should be small.

  std::vector<ui::AXPlatformNode*> nodes;
  for (int32_t target_id : target_ids) {
    if (ui::AXPlatformNode* node = GetFromNodeID(target_id)) {
      if (!base::Contains(nodes, node))
        nodes.push_back(node);
    }
  }

  return nodes;
}

std::set<ui::AXPlatformNode*>
BrowserAccessibility::GetSourceNodesForReverseRelations(
    ax::mojom::IntAttribute attr) {
  DCHECK(manager_);
  DCHECK(ui::IsNodeIdIntAttribute(attr));
  return GetNodesForNodeIdSet(
      manager_->ax_tree()->GetReverseRelations(attr, GetData().id));
}

std::set<ui::AXPlatformNode*>
BrowserAccessibility::GetSourceNodesForReverseRelations(
    ax::mojom::IntListAttribute attr) {
  DCHECK(manager_);
  DCHECK(ui::IsNodeIdIntListAttribute(attr));
  return GetNodesForNodeIdSet(
      manager_->ax_tree()->GetReverseRelations(attr, GetData().id));
}

const ui::AXUniqueId& BrowserAccessibility::GetUniqueId() const {
  // This is not the same as GetData().id which comes from Blink, because
  // those ids are only unique within the Blink process. We need one that is
  // unique for the browser process.
  return unique_id_;
}

std::string BrowserAccessibility::SubtreeToStringHelper(size_t level) {
  std::string result(level * 2, '+');
  result += ToString();
  result += '\n';

  for (InternalChildIterator it = InternalChildrenBegin();
       it != InternalChildrenEnd(); ++it) {
    BrowserAccessibility* child = it.get();
    DCHECK(child);
    result += child->SubtreeToStringHelper(level + 1);
  }

  return result;
}

void BrowserAccessibility::NotifyAccessibilityApiUsage() const {
  content::BrowserAccessibilityStateImpl::GetInstance()
      ->OnAccessibilityApiUsage();
}

const std::vector<gfx::NativeViewAccessible>
BrowserAccessibility::GetUIADirectChildrenInRange(
    ui::AXPlatformNodeDelegate* start,
    ui::AXPlatformNodeDelegate* end) {
  // This method is only called on Windows. Other platforms should not call it.
  // The BrowserAccessibilityWin subclass overrides this method.
  NOTREACHED();
  return {};
}

//
// AXPlatformNodeDelegate.
//

gfx::NativeViewAccessible BrowserAccessibility::GetParent() const {
  BrowserAccessibility* parent = PlatformGetParent();
  if (parent)
    return parent->GetNativeViewAccessible();

  WebAXPlatformTreeManagerDelegate* delegate =
      manager_->GetDelegateFromRootManager();
  if (!delegate)
    return nullptr;
  return delegate->AccessibilityGetNativeViewAccessible();
}

size_t BrowserAccessibility::GetChildCount() const {
  return PlatformChildCount();
}

gfx::NativeViewAccessible BrowserAccessibility::ChildAtIndex(size_t index) {
  BrowserAccessibility* child = PlatformGetChild(index);
  if (!child)
    return nullptr;
  return child->GetNativeViewAccessible();
}

gfx::NativeViewAccessible BrowserAccessibility::GetFirstChild() {
  BrowserAccessibility* child = PlatformGetFirstChild();
  if (!child)
    return nullptr;
  return child->GetNativeViewAccessible();
}

gfx::NativeViewAccessible BrowserAccessibility::GetLastChild() {
  BrowserAccessibility* child = PlatformGetLastChild();
  if (!child)
    return nullptr;
  return child->GetNativeViewAccessible();
}

gfx::NativeViewAccessible BrowserAccessibility::GetNextSibling() {
  BrowserAccessibility* sibling = PlatformGetNextSibling();
  if (!sibling)
    return nullptr;
  return sibling->GetNativeViewAccessible();
}

gfx::NativeViewAccessible BrowserAccessibility::GetPreviousSibling() {
  BrowserAccessibility* sibling = PlatformGetPreviousSibling();
  if (!sibling)
    return nullptr;
  return sibling->GetNativeViewAccessible();
}

bool BrowserAccessibility::IsLeaf() const {
  // According to the ARIA and Core-AAM specs:
  // https://w3c.github.io/aria/#button,
  // https://www.w3.org/TR/core-aam-1.1/#exclude_elements
  // button's children are presentational only and should be hidden from
  // screen readers. However, we cannot enforce the leafiness of buttons
  // because they may contain many rich, interactive descendants such as a day
  // in a calendar, and screen readers will need to interact with these
  // contents. See https://crbug.com/689204.
  // So we decided to not enforce the leafiness of buttons and expose all
  // children. The only exception to enforce leafiness is when the button has
  // a single text child and to prevent screen readers from double speak.
  if (GetRole() == ax::mojom::Role::kButton) {
    size_t child_count = InternalChildCount();
    return !child_count ||
           (child_count == 1 && InternalGetFirstChild()->IsText());
  }
  if (PlatformGetRootOfChildTree())
    return false;  // This object is hosting another tree.
  return node()->IsLeaf();
}

bool BrowserAccessibility::IsFocused() const {
  // TODO(nektar): Create an `ax_focus` class to share focus state between Views
  // and Web.
  return manager()->GetFocus() == this;
}

bool BrowserAccessibility::IsPlatformDocument() const {
  return ui::IsPlatformDocument(GetRole());
}

gfx::NativeViewAccessible BrowserAccessibility::GetLowestPlatformAncestor()
    const {
  BrowserAccessibility* lowest_platform_ancestor =
      PlatformGetLowestPlatformAncestor();
  if (lowest_platform_ancestor)
    return lowest_platform_ancestor->GetNativeViewAccessible();
  return nullptr;
}

gfx::NativeViewAccessible BrowserAccessibility::GetTextFieldAncestor() const {
  BrowserAccessibility* text_field_ancestor = PlatformGetTextFieldAncestor();
  if (text_field_ancestor)
    return text_field_ancestor->GetNativeViewAccessible();
  return nullptr;
}

gfx::NativeViewAccessible BrowserAccessibility::GetSelectionContainer() const {
  BrowserAccessibility* selection_container = PlatformGetSelectionContainer();
  if (selection_container)
    return selection_container->GetNativeViewAccessible();
  return nullptr;
}

gfx::NativeViewAccessible BrowserAccessibility::GetTableAncestor() const {
  BrowserAccessibility* table_ancestor =
      manager()->GetFromAXNode(node()->GetTableAncestor());
  if (table_ancestor)
    return table_ancestor->GetNativeViewAccessible();
  return nullptr;
}

BrowserAccessibility::PlatformChildIterator::PlatformChildIterator(
    const PlatformChildIterator& it)
    : parent_(it.parent_), platform_iterator(it.platform_iterator) {}

BrowserAccessibility::PlatformChildIterator::PlatformChildIterator(
    const BrowserAccessibility* parent,
    BrowserAccessibility* child)
    : parent_(parent), platform_iterator(parent, child) {
  DCHECK(parent);
}

BrowserAccessibility::PlatformChildIterator::~PlatformChildIterator() = default;

BrowserAccessibility::PlatformChildIterator&
BrowserAccessibility::PlatformChildIterator::operator++() {
  ++platform_iterator;
  return *this;
}

BrowserAccessibility::PlatformChildIterator&
BrowserAccessibility::PlatformChildIterator::operator++(int) {
  ++platform_iterator;
  return *this;
}

BrowserAccessibility::PlatformChildIterator&
BrowserAccessibility::PlatformChildIterator::operator--() {
  --platform_iterator;
  return *this;
}

BrowserAccessibility::PlatformChildIterator&
BrowserAccessibility::PlatformChildIterator::operator--(int) {
  --platform_iterator;
  return *this;
}

BrowserAccessibility* BrowserAccessibility::PlatformChildIterator::get() const {
  return platform_iterator.get();
}

gfx::NativeViewAccessible
BrowserAccessibility::PlatformChildIterator::GetNativeViewAccessible() const {
  return platform_iterator->GetNativeViewAccessible();
}

absl::optional<size_t>
BrowserAccessibility::PlatformChildIterator::GetIndexInParent() const {
  if (platform_iterator == parent_->PlatformChildrenEnd().platform_iterator)
    return parent_->PlatformChildCount();

  return platform_iterator->GetIndexInParent();
}

BrowserAccessibility& BrowserAccessibility::PlatformChildIterator::operator*()
    const {
  return *platform_iterator;
}

BrowserAccessibility* BrowserAccessibility::PlatformChildIterator::operator->()
    const {
  return platform_iterator.get();
}

std::unique_ptr<ui::AXPlatformNodeDelegate::ChildIterator>
BrowserAccessibility::ChildrenBegin() {
  return std::make_unique<PlatformChildIterator>(PlatformChildrenBegin());
}

std::unique_ptr<ui::AXPlatformNodeDelegate::ChildIterator>
BrowserAccessibility::ChildrenEnd() {
  return std::make_unique<PlatformChildIterator>(PlatformChildrenEnd());
}

gfx::NativeViewAccessible BrowserAccessibility::HitTestSync(
    int physical_pixel_x,
    int physical_pixel_y) const {
  BrowserAccessibility* accessible = manager_->CachingAsyncHitTest(
      gfx::Point(physical_pixel_x, physical_pixel_y));
  if (!accessible)
    return nullptr;

  return accessible->GetNativeViewAccessible();
}

gfx::NativeViewAccessible BrowserAccessibility::GetFocus() const {
  BrowserAccessibility* focused = manager()->GetFocus();
  if (!focused)
    return nullptr;

  return focused->GetNativeViewAccessible();
}

ui::AXPlatformNode* BrowserAccessibility::GetFromNodeID(int32_t id) {
  BrowserAccessibility* node = manager_->GetFromID(id);
  if (!node)
    return nullptr;

  return node->GetAXPlatformNode();
}

ui::AXPlatformNode* BrowserAccessibility::GetFromTreeIDAndNodeID(
    const ui::AXTreeID& ax_tree_id,
    int32_t id) {
  BrowserAccessibilityManager* manager =
      BrowserAccessibilityManager::FromID(ax_tree_id);
  if (!manager)
    return nullptr;

  BrowserAccessibility* node = manager->GetFromID(id);
  if (!node)
    return nullptr;

  return node->GetAXPlatformNode();
}

absl::optional<size_t> BrowserAccessibility::GetIndexInParent() {
  if (manager()->GetBrowserAccessibilityRoot() == this &&
      PlatformGetParent() == nullptr) {
    // If it is a root node of WebContent, it doesn't have a parent and a
    // valid index in parent. So it returns -1 in order to compute its
    // index at AXPlatformNodeBase.
    return absl::nullopt;
  }
  return node()->GetUnignoredIndexInParent();
}

gfx::AcceleratedWidget
BrowserAccessibility::GetTargetForNativeAccessibilityEvent() {
  WebAXPlatformTreeManagerDelegate* root_delegate =
      manager()->GetDelegateFromRootManager();
  if (!root_delegate)
    return gfx::kNullAcceleratedWidget;
  return root_delegate->AccessibilityGetAcceleratedWidget();
}

absl::optional<int> BrowserAccessibility::GetTableAriaColCount() const {
  return node()->GetTableAriaColCount();
}

absl::optional<int> BrowserAccessibility::GetTableAriaRowCount() const {
  return node()->GetTableAriaRowCount();
}

ui::AXPlatformNode* BrowserAccessibility::GetTableCaption() const {
  ui::AXNode* caption = node()->GetTableCaption();
  if (caption) {
    return const_cast<BrowserAccessibility*>(this)->GetFromNodeID(
        caption->id());
  }
  return nullptr;
}

bool BrowserAccessibility::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  // TODO(crbug.com/1049261): Move the ability to perform actions to
  // `AXTreeManager`.
  switch (data.action) {
    case ax::mojom::Action::kDoDefault:
      manager_->DoDefaultAction(*this);
      return true;
    case ax::mojom::Action::kFocus:
      manager_->SetFocus(*this);
      return true;
    case ax::mojom::Action::kScrollToPoint: {
      // Convert the target point from screen coordinates to frame coordinates.
      gfx::Point target =
          data.target_point - manager_->GetBrowserAccessibilityRoot()
                                  ->GetUnclippedScreenBoundsRect()
                                  .OffsetFromOrigin();
      manager_->ScrollToPoint(*this, target);
      return true;
    }
    case ax::mojom::Action::kScrollToMakeVisible:
      manager_->ScrollToMakeVisible(
          *this, data.target_rect, data.horizontal_scroll_alignment,
          data.vertical_scroll_alignment, data.scroll_behavior);
      return true;
    case ax::mojom::Action::kSetScrollOffset:
      manager_->SetScrollOffset(*this, data.target_point);
      return true;
    case ax::mojom::Action::kSetSelection: {
      ui::AXActionData selection = data;

      // Prioritize target_tree_id if it was provided, as it is possible on
      // some platforms (such as IAccessible2) to initiate a selection in a
      // different tree than the current node resides in, as long as the nodes
      // being selected share an AXTree with each other.
      BrowserAccessibilityManager* selection_manager = nullptr;
      if (selection.target_tree_id != ui::AXTreeIDUnknown()) {
        selection_manager =
            BrowserAccessibilityManager::FromID(selection.target_tree_id);
      } else {
        selection_manager = manager_;
      }
      DCHECK(selection_manager);

      // "data.anchor_offset" and "data.focus_offset" might need to be adjusted
      // if the anchor or the focus nodes include ignored children.
      const BrowserAccessibility* anchor_object =
          selection_manager->GetFromID(selection.anchor_node_id);
      DCHECK(anchor_object);
      if (!anchor_object->IsLeaf()) {
        DCHECK_GE(selection.anchor_offset, 0);
        const BrowserAccessibility* anchor_child =
            anchor_object->InternalGetChild(
                static_cast<uint32_t>(selection.anchor_offset));
        if (anchor_child) {
          selection.anchor_offset =
              static_cast<int>(anchor_child->node()->index_in_parent());
          selection.anchor_node_id = anchor_child->node()->parent()->id();
        } else {
          // Since the child was not found, the only alternative is that this is
          // an "after children" position.
          selection.anchor_offset =
              static_cast<int>(anchor_object->node()->children().size());
        }
      }

      const BrowserAccessibility* focus_object =
          selection_manager->GetFromID(selection.focus_node_id);
      DCHECK(focus_object);

      // Blink only supports selections between two nodes in the same tree.
      DCHECK_EQ(anchor_object->GetTreeData().tree_id,
                focus_object->GetTreeData().tree_id);
      if (!focus_object->IsLeaf()) {
        DCHECK_GE(selection.focus_offset, 0);
        const BrowserAccessibility* focus_child =
            focus_object->InternalGetChild(
                static_cast<uint32_t>(selection.focus_offset));
        if (focus_child) {
          selection.focus_offset =
              static_cast<int>(focus_child->node()->index_in_parent());
          selection.focus_node_id = focus_child->node()->parent()->id();
        } else {
          // Since the child was not found, the only alternative is that this is
          // an "after children" position.
          selection.focus_offset =
              static_cast<int>(focus_object->node()->children().size());
        }
      }

      selection_manager->SetSelection(selection);
      return true;
    }
    case ax::mojom::Action::kSetValue:
      manager_->SetValue(*this, data.value);
      return true;
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
      manager_->SetSequentialFocusNavigationStartingPoint(*this);
      return true;
    case ax::mojom::Action::kShowContextMenu:
      manager_->ShowContextMenu(*this);
      return true;
    case ax::mojom::Action::kIncrement:
      manager_->Increment(*this);
      return true;
    case ax::mojom::Action::kDecrement:
      manager_->Decrement(*this);
      return true;
    case ax::mojom::Action::kScrollBackward:
    case ax::mojom::Action::kScrollForward:
    case ax::mojom::Action::kScrollUp:
    case ax::mojom::Action::kScrollDown:
    case ax::mojom::Action::kScrollLeft:
    case ax::mojom::Action::kScrollRight:
      manager_->Scroll(*this, data.action);
      return true;
    default:
      return false;
  }
}

std::u16string BrowserAccessibility::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  // TODO(crbug.com/1049261): This is one of the few methods that won't be moved
  // to `AXNode` in the foreseeable future because the functionality it provides
  // is not immediately needed in Views.

  ContentClient* content_client = content::GetContentClient();

  int message_id = 0;
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      message_id = IDS_AX_IMAGE_ELIGIBLE_FOR_ANNOTATION;
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      message_id = IDS_AX_IMAGE_ANNOTATION_PENDING;
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      message_id = IDS_AX_IMAGE_ANNOTATION_ADULT;
      break;
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
      message_id = IDS_AX_IMAGE_ANNOTATION_NO_DESCRIPTION;
      break;
    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      return std::u16string();
  }

  DCHECK(message_id);

  return content_client->GetLocalizedString(message_id);
}

std::u16string
BrowserAccessibility::GetLocalizedRoleDescriptionForUnlabeledImage() const {
  // TODO(crbug.com/1049261): This is one of the few methods that won't be moved
  // to `AXNode` in the foreseeable future because the functionality it provides
  // is not immediately needed in Views.

  ContentClient* content_client = content::GetContentClient();
  return content_client->GetLocalizedString(
      IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION);
}

std::u16string BrowserAccessibility::GetLocalizedStringForLandmarkType() const {
  // This method is Web specific and thus cannot be move to `AXNode`.

  ContentClient* content_client = content::GetContentClient();

  switch (GetRole()) {
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kHeader:
      return content_client->GetLocalizedString(IDS_AX_ROLE_BANNER);

    case ax::mojom::Role::kComplementary:
      return content_client->GetLocalizedString(IDS_AX_ROLE_COMPLEMENTARY);

    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return content_client->GetLocalizedString(IDS_AX_ROLE_CONTENT_INFO);

    case ax::mojom::Role::kRegion:
      return content_client->GetLocalizedString(IDS_AX_ROLE_REGION);

    default:
      return {};
  }
}

std::u16string BrowserAccessibility::GetLocalizedStringForRoleDescription()
    const {
  // TODO(nektar): Move this method to `AXNode` if possible.

  // Localized role description strings live in ui/strings/ax_strings.grd
  ContentClient* content_client = content::GetContentClient();

  switch (GetRole()) {
    // Things which should never have a role description.
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kGenericContainer:
    case ax::mojom::Role::kIframePresentational:
    case ax::mojom::Role::kImeCandidate:
    case ax::mojom::Role::kInlineTextBox:
    case ax::mojom::Role::kLayoutTable:
    case ax::mojom::Role::kLayoutTableCell:
    case ax::mojom::Role::kLayoutTableRow:
    case ax::mojom::Role::kLineBreak:
    case ax::mojom::Role::kListMarker:
    case ax::mojom::Role::kRuby:
    case ax::mojom::Role::kRubyAnnotation:
    case ax::mojom::Role::kStaticText:
    case ax::mojom::Role::kUnknown:
      return {};

    // TODO(accessibility): Should any of these have a role description?
    case ax::mojom::Role::kAbbr:
    case ax::mojom::Role::kCaption:
    case ax::mojom::Role::kCanvas:
    case ax::mojom::Role::kCaret:
    case ax::mojom::Role::kCell:
    case ax::mojom::Role::kClient:
    case ax::mojom::Role::kColumn:
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kDesktop:
    case ax::mojom::Role::kFigcaption:
    case ax::mojom::Role::kGroup:
    case ax::mojom::Role::kIframe:
    case ax::mojom::Role::kLegend:
    case ax::mojom::Role::kKeyboard:
    case ax::mojom::Role::kLabelText:
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuListPopup:
    case ax::mojom::Role::kPane:
    case ax::mojom::Role::kParagraph:
    case ax::mojom::Role::kPdfRoot:
    case ax::mojom::Role::kPre:
    case ax::mojom::Role::kRow:
    case ax::mojom::Role::kScrollView:
    case ax::mojom::Role::kTableHeaderContainer:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kTitleBar:
    case ax::mojom::Role::kWebView:
    case ax::mojom::Role::kWindow:
      return {};

    // DPUB-ARIA Roles
    case ax::mojom::Role::kDocAbstract:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_ABSTRACT);
    case ax::mojom::Role::kDocAcknowledgments:
      return content_client->GetLocalizedString(
          IDS_AX_ROLE_DOC_ACKNOWLEDGMENTS);
    case ax::mojom::Role::kDocAfterword:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_AFTERWORD);
    case ax::mojom::Role::kDocAppendix:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_APPENDIX);
    case ax::mojom::Role::kDocBackLink:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_BACKLINK);
    case ax::mojom::Role::kDocBiblioEntry:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_BIBLIO_ENTRY);
    case ax::mojom::Role::kDocBibliography:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_BIBLIOGRAPHY);
    case ax::mojom::Role::kDocBiblioRef:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_BIBLIO_REF);
    case ax::mojom::Role::kDocChapter:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_CHAPTER);
    case ax::mojom::Role::kDocColophon:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_COLOPHON);
    case ax::mojom::Role::kDocConclusion:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_CONCLUSION);
    case ax::mojom::Role::kDocCover:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_COVER);
    case ax::mojom::Role::kDocCredit:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_CREDIT);
    case ax::mojom::Role::kDocCredits:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_CREDITS);
    case ax::mojom::Role::kDocDedication:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_DEDICATION);
    case ax::mojom::Role::kDocEndnote:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_ENDNOTE);
    case ax::mojom::Role::kDocEndnotes:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_ENDNOTES);
    case ax::mojom::Role::kDocEpigraph:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_EPIGRAPH);
    case ax::mojom::Role::kDocEpilogue:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_EPILOGUE);
    case ax::mojom::Role::kDocErrata:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_ERRATA);
    case ax::mojom::Role::kDocExample:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_EXAMPLE);
    case ax::mojom::Role::kDocFootnote:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_FOOTNOTE);
    case ax::mojom::Role::kDocForeword:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_FOREWORD);
    case ax::mojom::Role::kDocGlossary:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_GLOSSARY);
    case ax::mojom::Role::kDocGlossRef:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_GLOSS_REF);
    case ax::mojom::Role::kDocIndex:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_INDEX);
    case ax::mojom::Role::kDocIntroduction:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_INTRODUCTION);
    case ax::mojom::Role::kDocNoteRef:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_NOTE_REF);
    case ax::mojom::Role::kDocNotice:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_NOTICE);
    case ax::mojom::Role::kDocPageBreak:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_PAGE_BREAK);
    case ax::mojom::Role::kDocPageFooter:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_PAGE_FOOTER);
    case ax::mojom::Role::kDocPageHeader:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_PAGE_HEADER);
    case ax::mojom::Role::kDocPageList:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_PAGE_LIST);
    case ax::mojom::Role::kDocPart:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_PART);
    case ax::mojom::Role::kDocPreface:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_PREFACE);
    case ax::mojom::Role::kDocPrologue:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_PROLOGUE);
    case ax::mojom::Role::kDocPullquote:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_PULLQUOTE);
    case ax::mojom::Role::kDocQna:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_QNA);
    case ax::mojom::Role::kDocSubtitle:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_SUBTITLE);
    case ax::mojom::Role::kDocTip:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_TIP);
    case ax::mojom::Role::kDocToc:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOC_TOC);

    // Graphics ARIA Roles
    case ax::mojom::Role::kGraphicsDocument:
      return content_client->GetLocalizedString(IDS_AX_ROLE_GRAPHICS_DOCUMENT);
    case ax::mojom::Role::kGraphicsObject:
      return content_client->GetLocalizedString(IDS_AX_ROLE_GRAPHICS_OBJECT);
    case ax::mojom::Role::kGraphicsSymbol:
      return content_client->GetLocalizedString(IDS_AX_ROLE_GRAPHICS_SYMBOL);

    // MathML Roles
    case ax::mojom::Role::kMathMLMath:
      return content_client->GetLocalizedString(IDS_AX_ROLE_MATH);
    case ax::mojom::Role::kMathMLFraction:
    case ax::mojom::Role::kMathMLIdentifier:
    case ax::mojom::Role::kMathMLMultiscripts:
    case ax::mojom::Role::kMathMLNoneScript:
    case ax::mojom::Role::kMathMLNumber:
    case ax::mojom::Role::kMathMLOperator:
    case ax::mojom::Role::kMathMLOver:
    case ax::mojom::Role::kMathMLPrescriptDelimiter:
    case ax::mojom::Role::kMathMLRoot:
    case ax::mojom::Role::kMathMLRow:
    case ax::mojom::Role::kMathMLSquareRoot:
    case ax::mojom::Role::kMathMLStringLiteral:
    case ax::mojom::Role::kMathMLSub:
    case ax::mojom::Role::kMathMLSubSup:
    case ax::mojom::Role::kMathMLSup:
    case ax::mojom::Role::kMathMLTable:
    case ax::mojom::Role::kMathMLTableCell:
    case ax::mojom::Role::kMathMLTableRow:
    case ax::mojom::Role::kMathMLText:
    case ax::mojom::Role::kMathMLUnder:
    case ax::mojom::Role::kMathMLUnderOver:
      return {};

    // All Other Roles
    case ax::mojom::Role::kAlert:
      return content_client->GetLocalizedString(IDS_AX_ROLE_ALERT);
    case ax::mojom::Role::kAlertDialog:
      return content_client->GetLocalizedString(IDS_AX_ROLE_ALERT_DIALOG);
    case ax::mojom::Role::kApplication:
      return content_client->GetLocalizedString(IDS_AX_ROLE_APPLICATION);
    case ax::mojom::Role::kArticle:
      return content_client->GetLocalizedString(IDS_AX_ROLE_ARTICLE);
    case ax::mojom::Role::kAudio:
      // Android returns IDS_AX_MEDIA_AUDIO_ELEMENT, but the string is the same.
      return content_client->GetLocalizedString(IDS_AX_ROLE_AUDIO);
    case ax::mojom::Role::kBanner:
      return content_client->GetLocalizedString(IDS_AX_ROLE_BANNER);
    case ax::mojom::Role::kBlockquote:
      return content_client->GetLocalizedString(IDS_AX_ROLE_BLOCKQUOTE);
    case ax::mojom::Role::kButton:
      return content_client->GetLocalizedString(IDS_AX_ROLE_BUTTON);
    case ax::mojom::Role::kCheckBox:
      return content_client->GetLocalizedString(IDS_AX_ROLE_CHECK_BOX);
    case ax::mojom::Role::kCode:
      return content_client->GetLocalizedString(IDS_AX_ROLE_CODE);
    case ax::mojom::Role::kColorWell:
      return content_client->GetLocalizedString(IDS_AX_ROLE_COLOR_WELL);
    case ax::mojom::Role::kColumnHeader:
      return content_client->GetLocalizedString(IDS_AX_ROLE_COLUMN_HEADER);
    case ax::mojom::Role::kComboBoxSelect:
      // TODO(crbug.com/1362834): This is used for Mac AXRoleDescription. This
      // should be changed at the same time we map this role to
      // NSAccessibilityComboBoxRole.
      return content_client->GetLocalizedString(IDS_AX_ROLE_POP_UP_BUTTON);
    case ax::mojom::Role::kComment:
      return content_client->GetLocalizedString(IDS_AX_ROLE_COMMENT);
    case ax::mojom::Role::kComplementary:
      return content_client->GetLocalizedString(IDS_AX_ROLE_COMPLEMENTARY);
    case ax::mojom::Role::kContentDeletion:
      return content_client->GetLocalizedString(IDS_AX_ROLE_CONTENT_DELETION);
    case ax::mojom::Role::kContentInfo:
      return content_client->GetLocalizedString(IDS_AX_ROLE_CONTENT_INFO);
    case ax::mojom::Role::kContentInsertion:
      return content_client->GetLocalizedString(IDS_AX_ROLE_CONTENT_INSERTION);
    case ax::mojom::Role::kDate:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DATE);
    case ax::mojom::Role::kDateTime: {
      std::string input_type;
      if (GetStringAttribute(ax::mojom::StringAttribute::kInputType,
                             &input_type)) {
        if (input_type == "datetime-local") {
          return content_client->GetLocalizedString(
              IDS_AX_ROLE_DATE_TIME_LOCAL);
        } else if (input_type == "week") {
          return content_client->GetLocalizedString(IDS_AX_ROLE_WEEK);
        } else if (input_type == "month") {
          return content_client->GetLocalizedString(IDS_AX_ROLE_MONTH);
        }
      }
      return content_client->GetLocalizedString(IDS_AX_ROLE_DATE_TIME);
    }
    case ax::mojom::Role::kDefinition:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DEFINITION);
    case ax::mojom::Role::kDescriptionList:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DESCRIPTION_LIST);
    case ax::mojom::Role::kDescriptionListDetail:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DEFINITION);
    case ax::mojom::Role::kDescriptionListTerm:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DESCRIPTION_TERM);
    case ax::mojom::Role::kDetails:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DETAILS);
    case ax::mojom::Role::kDialog:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DIALOG);
    case ax::mojom::Role::kDirectory:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DIRECTORY);
    case ax::mojom::Role::kDisclosureTriangle:
      return content_client->GetLocalizedString(
          IDS_AX_ROLE_DISCLOSURE_TRIANGLE);
    case ax::mojom::Role::kDocument:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DOCUMENT);
    case ax::mojom::Role::kEmbeddedObject:
      return content_client->GetLocalizedString(IDS_AX_ROLE_EMBEDDED_OBJECT);
    case ax::mojom::Role::kEmphasis:
      return content_client->GetLocalizedString(IDS_AX_ROLE_EMPHASIS);
    case ax::mojom::Role::kFeed:
      return content_client->GetLocalizedString(IDS_AX_ROLE_FEED);
    case ax::mojom::Role::kFigure:
      return content_client->GetLocalizedString(IDS_AX_ROLE_FIGURE);
    case ax::mojom::Role::kFooter:
      return content_client->GetLocalizedString(IDS_AX_ROLE_FOOTER);
    case ax::mojom::Role::kFooterAsNonLandmark:
      return content_client->GetLocalizedString(IDS_AX_ROLE_FOOTER);
    case ax::mojom::Role::kForm:
      return content_client->GetLocalizedString(IDS_AX_ROLE_FORM);
    case ax::mojom::Role::kGrid:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TABLE);
    case ax::mojom::Role::kHeader:
      return content_client->GetLocalizedString(IDS_AX_ROLE_HEADER);
    case ax::mojom::Role::kHeaderAsNonLandmark:
      return content_client->GetLocalizedString(IDS_AX_ROLE_HEADER);
    case ax::mojom::Role::kHeading:
      return content_client->GetLocalizedString(IDS_AX_ROLE_HEADING);
    case ax::mojom::Role::kImage:
      return content_client->GetLocalizedString(IDS_AX_ROLE_GRAPHIC);
    case ax::mojom::Role::kInputTime:
      return content_client->GetLocalizedString(IDS_AX_ROLE_INPUT_TIME);
    case ax::mojom::Role::kLink:
      return content_client->GetLocalizedString(IDS_AX_ROLE_LINK);
    case ax::mojom::Role::kListBox:
      return content_client->GetLocalizedString(IDS_AX_ROLE_LIST_BOX);
    case ax::mojom::Role::kListGrid:
      return {};
    case ax::mojom::Role::kLog:
      return content_client->GetLocalizedString(IDS_AX_ROLE_LOG);
    case ax::mojom::Role::kMain:
      return content_client->GetLocalizedString(IDS_AX_ROLE_MAIN_CONTENT);
    case ax::mojom::Role::kMark:
      return content_client->GetLocalizedString(IDS_AX_ROLE_MARK);
    case ax::mojom::Role::kMarquee:
      return content_client->GetLocalizedString(IDS_AX_ROLE_MARQUEE);
    case ax::mojom::Role::kMath:
      return content_client->GetLocalizedString(IDS_AX_ROLE_MATH);
    case ax::mojom::Role::kMenu:
      return content_client->GetLocalizedString(IDS_AX_ROLE_MENU);
    case ax::mojom::Role::kMenuBar:
      return content_client->GetLocalizedString(IDS_AX_ROLE_MENU_BAR);
    case ax::mojom::Role::kMenuItem:
      return content_client->GetLocalizedString(IDS_AX_ROLE_MENU_ITEM);
    case ax::mojom::Role::kMenuItemCheckBox:
      return {};
    case ax::mojom::Role::kMenuItemRadio:
      return {};
    case ax::mojom::Role::kMeter:
      return content_client->GetLocalizedString(IDS_AX_ROLE_METER);
    case ax::mojom::Role::kNavigation:
      return content_client->GetLocalizedString(IDS_AX_ROLE_NAVIGATIONAL_LINK);
    case ax::mojom::Role::kNote:
      return content_client->GetLocalizedString(IDS_AX_ROLE_NOTE);
    case ax::mojom::Role::kPdfActionableHighlight:
      return content_client->GetLocalizedString(IDS_AX_ROLE_PDF_HIGHLIGHT);
    case ax::mojom::Role::kPluginObject:
      return content_client->GetLocalizedString(IDS_AX_ROLE_EMBEDDED_OBJECT);
    case ax::mojom::Role::kPopUpButton:
      return content_client->GetLocalizedString(IDS_AX_ROLE_POP_UP_BUTTON);
    case ax::mojom::Role::kPortal:
      return {};
    case ax::mojom::Role::kProgressIndicator:
      return content_client->GetLocalizedString(IDS_AX_ROLE_PROGRESS_INDICATOR);
    case ax::mojom::Role::kRadioButton:
      return content_client->GetLocalizedString(IDS_AX_ROLE_RADIO);
    case ax::mojom::Role::kRadioGroup:
      return content_client->GetLocalizedString(IDS_AX_ROLE_RADIO_GROUP);
    case ax::mojom::Role::kRegion:
      return content_client->GetLocalizedString(IDS_AX_ROLE_REGION);
    case ax::mojom::Role::kRootWebArea:
      // There is IDS_AX_ROLE_WEB_AREA, but only the mac seems to use it.
      return {};
    case ax::mojom::Role::kRowGroup:
      return content_client->GetLocalizedString(IDS_AX_ROLE_ROW_GROUP);
    case ax::mojom::Role::kRowHeader:
      return content_client->GetLocalizedString(IDS_AX_ROLE_ROW_HEADER);
    case ax::mojom::Role::kScrollBar:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SCROLL_BAR);
    case ax::mojom::Role::kSearch:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SEARCH);
    case ax::mojom::Role::kSearchBox:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SEARCH_BOX);
    case ax::mojom::Role::kSection:
      // While there is an IDS_AX_ROLE_SECTION, no one seems to be using it.
      return {};
    case ax::mojom::Role::kSlider:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SLIDER);
    case ax::mojom::Role::kSpinButton:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SPIN_BUTTON);
    case ax::mojom::Role::kSplitter:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SPLITTER);
    case ax::mojom::Role::kStatus:
      return content_client->GetLocalizedString(IDS_AX_ROLE_STATUS);
    case ax::mojom::Role::kStrong:
      return content_client->GetLocalizedString(IDS_AX_ROLE_STRONG);
    case ax::mojom::Role::kSubscript:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SUBSCRIPT);
    case ax::mojom::Role::kSuggestion:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SUGGESTION);
    case ax::mojom::Role::kSuperscript:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SUPERSCRIPT);
    case ax::mojom::Role::kSvgRoot:
      return content_client->GetLocalizedString(IDS_AX_ROLE_GRAPHIC);
    case ax::mojom::Role::kSwitch:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SWITCH);
    case ax::mojom::Role::kTab:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TAB);
    case ax::mojom::Role::kTabList:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TAB_LIST);
    case ax::mojom::Role::kTabPanel:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TAB_PANEL);
    case ax::mojom::Role::kTable:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TABLE);
    case ax::mojom::Role::kTerm:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DESCRIPTION_TERM);
    case ax::mojom::Role::kTextField: {
      std::string input_type;
      if (GetStringAttribute(ax::mojom::StringAttribute::kInputType,
                             &input_type)) {
        if (input_type == "email") {
          return content_client->GetLocalizedString(IDS_AX_ROLE_EMAIL);
        } else if (input_type == "tel") {
          return content_client->GetLocalizedString(IDS_AX_ROLE_TELEPHONE);
        } else if (input_type == "url") {
          return content_client->GetLocalizedString(IDS_AX_ROLE_URL);
        }
      }
      return {};
    }
    case ax::mojom::Role::kTime:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TIME);
    case ax::mojom::Role::kTimer:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TIMER);
    case ax::mojom::Role::kToggleButton:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TOGGLE_BUTTON);
    case ax::mojom::Role::kToolbar:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TOOLBAR);
    case ax::mojom::Role::kTooltip:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TOOLTIP);
    case ax::mojom::Role::kTree:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TREE);
    case ax::mojom::Role::kTreeGrid:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TREE_GRID);
    case ax::mojom::Role::kTreeItem:
      return content_client->GetLocalizedString(IDS_AX_ROLE_TREE_ITEM);
    case ax::mojom::Role::kVideo:
      // Android returns IDS_AX_MEDIA_VIDEO_ELEMENT.
      return {};
  }
}

std::u16string BrowserAccessibility::GetStyleNameAttributeAsLocalizedString()
    const {
  // This method is Web specific and thus cannot be moved to `AXNode`.
  const BrowserAccessibility* current_node = this;
  while (current_node) {
    if (current_node->GetRole() == ax::mojom::Role::kMark) {
      ContentClient* content_client = content::GetContentClient();
      return content_client->GetLocalizedString(IDS_AX_ROLE_MARK);
    }
    current_node = current_node->PlatformGetParent();
  }
  return {};
}

bool BrowserAccessibility::ShouldIgnoreHoveredStateForTesting() {
  BrowserAccessibilityStateImpl* accessibility_state =
      BrowserAccessibilityStateImpl::GetInstance();
  return accessibility_state->disable_hot_tracking_for_testing();
}

absl::optional<int> BrowserAccessibility::GetPosInSet() const {
  return node()->GetPosInSet();
}

absl::optional<int> BrowserAccessibility::GetSetSize() const {
  return node()->GetSetSize();
}

bool BrowserAccessibility::IsInListMarker() const {
  return node()->IsInListMarker();
}

BrowserAccessibility* BrowserAccessibility::GetCollapsedMenuListSelectAncestor()
    const {
  ui::AXNode* popup_button = node()->GetCollapsedMenuListSelectAncestor();
  return manager()->GetFromAXNode(popup_button);
}

std::string BrowserAccessibility::ToString() const {
  return GetData().ToString();
}

bool BrowserAccessibility::SetHypertextSelection(int start_offset,
                                                 int end_offset) {
  // TODO(nektar): Move to `AXNode` as soon as hypertext computation is fully
  // migrated to that class.
  manager()->SetSelection(AXRange(CreatePositionForSelectionAt(start_offset),
                                  CreatePositionForSelectionAt(end_offset)));
  return true;
}

BrowserAccessibility* BrowserAccessibility::PlatformGetRootOfChildTree() const {
  std::string child_tree_id;
  if (!GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId,
                          &child_tree_id)) {
    return nullptr;
  }
  DCHECK_EQ(node()->children().size(), 0u)
      << "A node should not have both children and a child tree.";

  BrowserAccessibilityManager* child_manager =
      BrowserAccessibilityManager::FromID(
          ui::AXTreeID::FromString(child_tree_id));
  if (child_manager &&
      child_manager->GetBrowserAccessibilityRoot()->PlatformGetParent() == this)
    return child_manager->GetBrowserAccessibilityRoot();
  return nullptr;
}

ui::TextAttributeList BrowserAccessibility::ComputeTextAttributes() const {
  return ui::TextAttributeList();
}

ui::TextAttributeMap BrowserAccessibility::GetSpellingAndGrammarAttributes()
    const {
  // TODO(crbug.com/1049261): This is one of the few methods that won't be moved
  // to `AXNode` in the foreseeable future because the functionality it provides
  // is not immediately needed in Views.

  ui::TextAttributeMap spelling_attributes;
  if (IsText()) {
    const std::vector<int32_t>& marker_types =
        GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
    const std::vector<int32_t>& highlight_types =
        GetIntListAttribute(ax::mojom::IntListAttribute::kHighlightTypes);
    const std::vector<int>& marker_starts =
        GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts);
    const std::vector<int>& marker_ends =
        GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);
    for (size_t i = 0; i < marker_types.size(); ++i) {
      bool is_spelling_error =
          (marker_types[i] &
           static_cast<int32_t>(ax::mojom::MarkerType::kSpelling)) ||
          ((marker_types[i] &
            static_cast<int32_t>(ax::mojom::MarkerType::kHighlight)) &&
           highlight_types[i] ==
               static_cast<int32_t>(ax::mojom::HighlightType::kSpellingError));
      bool is_grammar_error =
          (marker_types[i] &
           static_cast<int32_t>(ax::mojom::MarkerType::kGrammar)) ||
          ((marker_types[i] &
            static_cast<int32_t>(ax::mojom::MarkerType::kHighlight)) &&
           highlight_types[i] ==
               static_cast<int32_t>(ax::mojom::HighlightType::kGrammarError));

      if (!is_spelling_error && !is_grammar_error)
        continue;

      ui::TextAttributeList start_attributes;
      if (is_spelling_error && is_grammar_error)
        start_attributes.push_back(
            std::make_pair("invalid", "spelling,grammar"));
      else if (is_spelling_error)
        start_attributes.push_back(std::make_pair("invalid", "spelling"));
      else if (is_grammar_error)
        start_attributes.push_back(std::make_pair("invalid", "grammar"));

      int start_offset = marker_starts[i];
      int end_offset = marker_ends[i];
      spelling_attributes[start_offset] = start_attributes;
      spelling_attributes[end_offset] = ui::TextAttributeList();
    }
  }

  // In the case of a native text field, text marker information, such as
  // misspellings, need to be collected from all the text field's descendants
  // and exposed on the text field itself. Otherwise, assistive software (AT)
  // won't be able to see them because the native field's descendants are an
  // implementation detail that is hidden from AT.
  if (IsAtomicTextField()) {
    int start_offset = 0;
    for (BrowserAccessibility* static_text =
             BrowserAccessibilityManager::NextTextOnlyObject(
                 InternalGetFirstChild());
         static_text; static_text = static_text->InternalGetNextSibling()) {
      ui::TextAttributeMap text_spelling_attributes =
          static_text->GetSpellingAndGrammarAttributes();
      for (auto& attribute : text_spelling_attributes) {
        spelling_attributes[start_offset + attribute.first] =
            std::move(attribute.second);
      }
      start_offset += static_cast<int>(static_text->GetHypertext().length());
    }
  }

  return spelling_attributes;
}

// static
void BrowserAccessibility::MergeSpellingAndGrammarIntoTextAttributes(
    const ui::TextAttributeMap& spelling_attributes,
    int start_offset,
    ui::TextAttributeMap* text_attributes) {
  if (!text_attributes) {
    NOTREACHED();
    return;
  }

  ui::TextAttributeList prev_attributes;
  for (const auto& spelling_attribute : spelling_attributes) {
    int offset = start_offset + spelling_attribute.first;
    auto iterator = text_attributes->find(offset);
    if (iterator == text_attributes->end()) {
      text_attributes->emplace(offset, prev_attributes);
      iterator = text_attributes->find(offset);
    } else {
      prev_attributes = iterator->second;
    }

    ui::TextAttributeList& existing_attributes = iterator->second;
    // There might be a spelling attribute already in the list of text
    // attributes, originating from "aria-invalid", that is being overwritten
    // by a spelling marker. If it already exists, prefer it over this
    // automatically computed attribute.
    if (!HasInvalidAttribute(existing_attributes)) {
      // Does not exist -- insert our own.
      existing_attributes.insert(existing_attributes.end(),
                                 spelling_attribute.second.begin(),
                                 spelling_attribute.second.end());
    }
  }
}

ui::TextAttributeMap BrowserAccessibility::ComputeTextAttributeMap(
    const ui::TextAttributeList& default_attributes) const {
  // TODO(crbug.com/1049261): This is one of the few methods that won't be moved
  // to `AXNode` in the foreseeable future because the functionality it provides
  // is not immediately needed in Views.

  ui::TextAttributeMap attributes_map;
  if (IsLeaf()) {
    attributes_map[0] = default_attributes;
    const ui::TextAttributeMap spelling_attributes =
        GetSpellingAndGrammarAttributes();
    MergeSpellingAndGrammarIntoTextAttributes(
        spelling_attributes, 0 /* start_offset */, &attributes_map);
    return attributes_map;
  }

  DCHECK(PlatformChildCount());

  int start_offset = 0;
  for (const auto& child : PlatformChildren()) {
    ui::TextAttributeList attributes(child.ComputeTextAttributes());

    if (attributes_map.empty()) {
      attributes_map[start_offset] = attributes;
    } else {
      // Only add the attributes for this child if we are at the start of a new
      // style span.
      ui::TextAttributeList previous_attributes =
          attributes_map.rbegin()->second;
      // Must check the size, otherwise if attributes is a subset of
      // prev_attributes, they would appear to be equal.
      if (!base::ranges::equal(attributes, previous_attributes)) {
        attributes_map[start_offset] = attributes;
      }
    }

    if (child.IsText()) {
      const ui::TextAttributeMap spelling_attributes =
          child.GetSpellingAndGrammarAttributes();
      MergeSpellingAndGrammarIntoTextAttributes(spelling_attributes,
                                                start_offset, &attributes_map);
      start_offset += child.GetHypertext().length();
    } else {
      start_offset += 1;
    }
  }

  return attributes_map;
}

// static
bool BrowserAccessibility::HasInvalidAttribute(
    const ui::TextAttributeList& attributes) {
  return base::Contains(attributes, "invalid", &ui::TextAttribute::first);
}

static bool HasListAncestor(const BrowserAccessibility* node) {
  if (node == nullptr)
    return false;

  if (ui::IsStaticList(node->GetRole()))
    return true;

  return HasListAncestor(node->InternalGetParent());
}

static bool HasListDescendant(const BrowserAccessibility* current,
                              const BrowserAccessibility* root) {
  // Do not check the root when looking for a list descendant.
  if (current != root && ui::IsStaticList(current->GetRole()))
    return true;

  for (auto it = current->InternalChildrenBegin();
       it != current->InternalChildrenEnd(); ++it) {
    if (HasListDescendant(it.get(), root))
      return true;
  }
  return false;
}

bool BrowserAccessibility::IsHierarchicalList() const {
  // TODO(nektar): Move this method to `AXNode` in the immediate future.
  if (!ui::IsStaticList(GetRole()))
    return false;
  return HasListDescendant(this, this) || HasListAncestor(InternalGetParent());
}

}  // namespace content
