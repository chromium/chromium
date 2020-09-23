// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include <cstddef>

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/common/ax_serialization_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {

#if !defined(PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL)
// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibility();
}
#endif

// static
BrowserAccessibility* BrowserAccessibility::FromAXPlatformNodeDelegate(
    ui::AXPlatformNodeDelegate* delegate) {
  if (!delegate || !delegate->IsWebContent())
    return nullptr;
  return static_cast<BrowserAccessibility*>(delegate);
}

BrowserAccessibility::BrowserAccessibility() = default;

BrowserAccessibility::~BrowserAccessibility() = default;

namespace {

const BrowserAccessibility* GetTextContainerForPlainTextField(
    const BrowserAccessibility& text_field) {
  DCHECK(text_field.IsPlainTextField());
  DCHECK_EQ(1u, text_field.InternalChildCount());
  // Text fields wrap their static text and inline text boxes in generic
  // containers, and some, like input type=search, wrap the wrapper as well.
  // Structure is like this:
  // Text field
  // -- Generic container
  // ---- Generic container  (optional, only occurs in some controls)
  // ------ Static text   <-- (optional, does not exist if field is empty)
  // -------- Inline text box children (can be multiple)
  // This method will return the lowest generic container.
  const BrowserAccessibility* child = text_field.InternalGetFirstChild();
  DCHECK_EQ(child->GetRole(), ax::mojom::Role::kGenericContainer);
  DCHECK_LE(child->InternalChildCount(), 1u);
  if (child->InternalChildCount() == 1) {
    const BrowserAccessibility* grand_child = child->InternalGetFirstChild();
    if (grand_child->GetRole() == ax::mojom::Role::kGenericContainer) {
      DCHECK_EQ(grand_child->InternalGetFirstChild()->GetRole(),
                ax::mojom::Role::kStaticText);
      return grand_child;
    }
    DCHECK_EQ(child->InternalGetFirstChild()->GetRole(),
              ax::mojom::Role::kStaticText);
  }
  return child;
}

int GetBoundaryTextOffsetInsideBaseAnchor(
    ax::mojom::MoveDirection direction,
    const BrowserAccessibilityPosition::AXPositionInstance& base,
    const BrowserAccessibilityPosition::AXPositionInstance& position) {
  if (base->GetAnchor() == position->GetAnchor())
    return position->text_offset();

  // If the position is outside the anchor of the base position, then return
  // the first or last position in the same direction.
  switch (direction) {
    case ax::mojom::MoveDirection::kNone:
      NOTREACHED();
      return position->text_offset();
    case ax::mojom::MoveDirection::kBackward:
      return base->CreatePositionAtStartOfAnchor()->text_offset();
    case ax::mojom::MoveDirection::kForward:
      return base->CreatePositionAtEndOfAnchor()->text_offset();
  }
}

}  // namespace

void BrowserAccessibility::Init(BrowserAccessibilityManager* manager,
                                ui::AXNode* node) {
  DCHECK(manager);
  DCHECK(node);
  manager_ = manager;
  node_ = node;
}

bool BrowserAccessibility::PlatformIsLeaf() const {
  // TODO(nektar): Remove in favor of IsLeaf.
  return IsLeaf();
}

bool BrowserAccessibility::CanFireEvents() const {
  // Allow events unless this object would be trimmed away.
  return !IsChildOfLeaf();
}

ui::AXPlatformNode* BrowserAccessibility::GetAXPlatformNode() const {
  // Not all BrowserAccessibility subclasses can return an AXPlatformNode yet.
  // So, here we just return nullptr.
  return nullptr;
}

uint32_t BrowserAccessibility::PlatformChildCount() const {
  if (PlatformIsLeaf())
    return 0;
  return PlatformGetRootOfChildTree() ? 1 : InternalChildCount();
}

BrowserAccessibility* BrowserAccessibility::PlatformGetParent() const {
  ui::AXNode* parent = node()->GetUnignoredParent();
  if (parent)
    return manager()->GetFromAXNode(parent);

  return manager()->GetParentNodeFromParentTree();
}

BrowserAccessibility* BrowserAccessibility::PlatformGetFirstChild() const {
  return PlatformGetChild(0);
}

BrowserAccessibility* BrowserAccessibility::PlatformGetLastChild() const {
  BrowserAccessibility* child_tree_root = PlatformGetRootOfChildTree();
  return child_tree_root ? child_tree_root : InternalGetLastChild();
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

BrowserAccessibility* BrowserAccessibility::PlatformGetSelectionContainer()
    const {
  BrowserAccessibility* container = PlatformGetParent();
  while (container &&
         !ui::IsContainerWithSelectableChildren(container->GetRole())) {
    container = container->PlatformGetParent();
  }
  return container;
}

bool BrowserAccessibility::IsDescendantOf(
    const BrowserAccessibility* ancestor) const {
  if (!ancestor)
    return false;

  if (this == ancestor)
    return true;

  if (PlatformGetParent())
    return PlatformGetParent()->IsDescendantOf(ancestor);

  return false;
}

bool BrowserAccessibility::IsDocument() const {
  return GetRole() == ax::mojom::Role::kRootWebArea ||
         GetRole() == ax::mojom::Role::kWebArea;
}

bool BrowserAccessibility::IsIgnored() const {
  return node()->IsIgnored();
}

bool BrowserAccessibility::IsLineBreakObject() const {
  return node()->IsLineBreak();
}

BrowserAccessibility* BrowserAccessibility::PlatformGetChild(
    uint32_t child_index) const {
  BrowserAccessibility* child_tree_root = PlatformGetRootOfChildTree();
  if (child_tree_root) {
    // A node with a child tree has only one child.
    return child_index ? nullptr : child_tree_root;
  }
  return InternalGetChild(child_index);
}

BrowserAccessibility* BrowserAccessibility::PlatformGetClosestPlatformObject()
    const {
  BrowserAccessibility* platform_object =
      const_cast<BrowserAccessibility*>(this);
  while (platform_object && platform_object->IsChildOfLeaf())
    platform_object = platform_object->InternalGetParent();

  DCHECK(platform_object);
  return platform_object;
}

bool BrowserAccessibility::IsPreviousSiblingOnSameLine() const {
  const BrowserAccessibility* previous_sibling = PlatformGetPreviousSibling();
  if (!previous_sibling)
    return false;

  // Line linkage information might not be provided on non-leaf objects.
  const BrowserAccessibility* leaf_object = PlatformDeepestFirstChild();
  if (!leaf_object)
    leaf_object = this;

  int32_t previous_on_line_id;
  if (leaf_object->GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                                   &previous_on_line_id)) {
    const BrowserAccessibility* previous_on_line =
        manager()->GetFromID(previous_on_line_id);
    // In the case of a static text sibling, the object designated to be the
    // previous object on this line might be one of its children, i.e. the last
    // inline text box.
    return previous_on_line &&
           previous_on_line->IsDescendantOf(previous_sibling);
  }
  return false;
}

bool BrowserAccessibility::IsNextSiblingOnSameLine() const {
  const BrowserAccessibility* next_sibling = PlatformGetNextSibling();
  if (!next_sibling)
    return false;

  // Line linkage information might not be provided on non-leaf objects.
  const BrowserAccessibility* leaf_object = PlatformDeepestLastChild();
  if (!leaf_object)
    leaf_object = this;

  int32_t next_on_line_id;
  if (leaf_object->GetIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                                   &next_on_line_id)) {
    const BrowserAccessibility* next_on_line =
        manager()->GetFromID(next_on_line_id);
    // In the case of a static text sibling, the object designated to be the
    // next object on this line might be one of its children, i.e. the first
    // inline text box.
    return next_on_line && next_on_line->IsDescendantOf(next_sibling);
  }
  return false;
}

BrowserAccessibility* BrowserAccessibility::PlatformDeepestFirstChild() const {
  if (!PlatformChildCount())
    return nullptr;

  BrowserAccessibility* deepest_child = PlatformGetFirstChild();
  while (deepest_child->PlatformChildCount())
    deepest_child = deepest_child->PlatformGetFirstChild();

  return deepest_child;
}

BrowserAccessibility* BrowserAccessibility::PlatformDeepestLastChild() const {
  if (!PlatformChildCount())
    return nullptr;

  BrowserAccessibility* deepest_child = PlatformGetLastChild();
  while (deepest_child->PlatformChildCount()) {
    deepest_child = deepest_child->PlatformGetLastChild();
  }

  return deepest_child;
}

BrowserAccessibility* BrowserAccessibility::InternalDeepestFirstChild() const {
  if (!InternalChildCount())
    return nullptr;

  BrowserAccessibility* deepest_child = InternalGetFirstChild();
  while (deepest_child->InternalChildCount())
    deepest_child = deepest_child->InternalGetFirstChild();

  return deepest_child;
}

BrowserAccessibility* BrowserAccessibility::InternalDeepestLastChild() const {
  if (!InternalChildCount())
    return nullptr;

  BrowserAccessibility* deepest_child = InternalGetLastChild();
  while (deepest_child->InternalChildCount())
    deepest_child = deepest_child->InternalGetLastChild();

  return deepest_child;
}

uint32_t BrowserAccessibility::InternalChildCount() const {
  return node_->GetUnignoredChildCount();
}

BrowserAccessibility* BrowserAccessibility::InternalGetChild(
    uint32_t child_index) const {
  ui::AXNode* child_node = node_->GetUnignoredChildAtIndex(child_index);
  if (!child_node)
    return nullptr;

  return manager_->GetFromAXNode(child_node);
}

BrowserAccessibility* BrowserAccessibility::InternalGetParent() const {
  ui::AXNode* child_node = node_->GetUnignoredParent();
  if (!child_node)
    return nullptr;

  return manager_->GetFromAXNode(child_node);
}

BrowserAccessibility* BrowserAccessibility::InternalGetFirstChild() const {
  return InternalGetChild(0);
}

BrowserAccessibility* BrowserAccessibility::InternalGetLastChild() const {
  ui::AXNode* child_node = node_->GetLastUnignoredChild();
  if (!child_node)
    return nullptr;

  return manager_->GetFromAXNode(child_node);
}

BrowserAccessibility* BrowserAccessibility::InternalGetNextSibling() const {
  ui::AXNode* child_node = node_->GetNextUnignoredSibling();
  if (!child_node)
    return nullptr;

  return manager_->GetFromAXNode(child_node);
}

BrowserAccessibility* BrowserAccessibility::InternalGetPreviousSibling() const {
  ui::AXNode* child_node = node_->GetPreviousUnignoredSibling();
  if (!child_node)
    return nullptr;

  return manager_->GetFromAXNode(child_node);
}

BrowserAccessibility::InternalChildIterator
BrowserAccessibility::InternalChildrenBegin() const {
  return InternalChildIterator(this, InternalGetFirstChild());
}

BrowserAccessibility::InternalChildIterator
BrowserAccessibility::InternalChildrenEnd() const {
  return InternalChildIterator(this, nullptr);
}

int32_t BrowserAccessibility::GetId() const {
  return node()->id();
}

gfx::RectF BrowserAccessibility::GetLocation() const {
  return GetData().relative_bounds.bounds;
}

ax::mojom::Role BrowserAccessibility::GetRole() const {
  return GetData().role;
}

int32_t BrowserAccessibility::GetState() const {
  return GetData().state;
}

const BrowserAccessibility::HtmlAttributes&
BrowserAccessibility::GetHtmlAttributes() const {
  return GetData().html_attributes;
}

gfx::Rect BrowserAccessibility::GetClippedScreenBoundsRect(
    ui::AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(ui::AXCoordinateSystem::kScreenDIPs,
                       ui::AXClippingBehavior::kClipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetUnclippedScreenBoundsRect(
    ui::AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(ui::AXCoordinateSystem::kScreenDIPs,
                       ui::AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetClippedRootFrameBoundsRect(
    ui::AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(ui::AXCoordinateSystem::kRootFrame,
                       ui::AXClippingBehavior::kClipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetUnclippedRootFrameBoundsRect(
    ui::AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(ui::AXCoordinateSystem::kRootFrame,
                       ui::AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect BrowserAccessibility::GetClippedFrameBoundsRect(
    ui::AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(ui::AXCoordinateSystem::kFrame,
                       ui::AXClippingBehavior::kUnclipped, offscreen_result);
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

  const base::string16& text_str = GetHypertext();
  if (effective_start_offset < 0 ||
      effective_start_offset >= static_cast<int>(text_str.size()))
    return gfx::Rect();
  if (effective_end_offset < 0 ||
      effective_end_offset > static_cast<int>(text_str.size()))
    return gfx::Rect();

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

  if (coordinate_system == ui::AXCoordinateSystem::kScreenPhysicalPixels) {
    // Convert to physical pixels.
    if (!IsUseZoomForDSFEnabled()) {
      bounds =
          gfx::ScaleToEnclosingRect(bounds, manager()->device_scale_factor());
    }
  }

  return bounds;
}

gfx::Rect BrowserAccessibility::GetRootFrameHypertextRangeBoundsRect(
    int start,
    int len,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  DCHECK_GE(start, 0);
  DCHECK_GE(len, 0);

  // Standard text fields such as textarea have an embedded div inside them that
  // holds all the text.
  // TODO(nektar): This is fragile! Replace with code that flattens tree.
  if (IsPlainTextField() && InternalChildCount() == 1) {
    return GetTextContainerForPlainTextField(*this)
        ->GetRootFrameHypertextRangeBoundsRect(start, len, clipping_behavior,
                                               offscreen_result);
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
        child->GetInlineTextRect(local_start, local_end, child_length),
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
  const int inner_text_length = GetInnerText().length();
  if (start_offset < 0 || end_offset > inner_text_length ||
      start_offset > end_offset)
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
  if (GetRole() == ax::mojom::Role::kInlineTextBox) {
    return RelativeToAbsoluteBounds(
        GetInlineTextRect(start_offset, end_offset, GetInnerText().length()),
        coordinate_system, clipping_behavior, offscreen_result);
  }

  if (IsPlainTextField() && InternalChildCount() == 1) {
    return GetTextContainerForPlainTextField(*this)->RelativeToAbsoluteBounds(
        GetInlineTextRect(start_offset, end_offset, GetInnerText().length()),
        coordinate_system, clipping_behavior, offscreen_result);
  }

  gfx::Rect bounds;
  int child_offset_in_parent = 0;
  for (InternalChildIterator it = InternalChildrenBegin();
       it != InternalChildrenEnd(); ++it) {
    const BrowserAccessibility* browser_accessibility_child = it.get();
    const int child_inner_text_length =
        browser_accessibility_child->GetInnerText().length();

    // The text bounds queried are not in this subtree; skip it and continue.
    const int child_start_offset =
        std::max(start_offset - child_offset_in_parent, 0);
    if (child_start_offset > child_inner_text_length) {
      child_offset_in_parent += child_inner_text_length;
      continue;
    }

    // The text bounds queried have already been gathered; short circuit.
    const int child_end_offset =
        std::min(end_offset - child_offset_in_parent, child_inner_text_length);
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

    child_offset_in_parent += child_inner_text_length;
  }

  return bounds;
}

gfx::RectF BrowserAccessibility::GetInlineTextRect(const int start_offset,
                                                   const int end_offset,
                                                   const int max_length) const {
  DCHECK(start_offset >= 0 && end_offset >= 0 && start_offset <= end_offset);
  int local_start_offset = start_offset, local_end_offset = end_offset;
  const std::vector<int32_t>& character_offsets =
      GetIntListAttribute(ax::mojom::IntListAttribute::kCharacterOffsets);
  const int character_offsets_length = character_offsets.size();
  if (character_offsets_length < max_length) {
    // Blink might not return pixel offsets for all characters. Clamp the
    // character range to be within the number of provided pixels.
    local_start_offset = std::min(local_start_offset, character_offsets_length);
    local_end_offset = std::min(local_end_offset, character_offsets_length);
  }

  const int start_pixel_offset =
      local_start_offset > 0 ? character_offsets[local_start_offset - 1] : 0;
  const int end_pixel_offset =
      local_end_offset > 0 ? character_offsets[local_end_offset - 1] : 0;
  const int max_pixel_offset =
      character_offsets_length > 0
          ? character_offsets[character_offsets_length - 1]
          : 0;
  const gfx::RectF location = GetLocation();
  const int location_width = location.width();
  const int location_height = location.height();

  gfx::RectF bounds;
  switch (static_cast<ax::mojom::WritingDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection))) {
    case ax::mojom::WritingDirection::kNone:
    case ax::mojom::WritingDirection::kLtr:
      bounds =
          gfx::RectF(start_pixel_offset, 0,
                     end_pixel_offset - start_pixel_offset, location_height);
      break;
    case ax::mojom::WritingDirection::kRtl: {
      const int left = max_pixel_offset - end_pixel_offset;
      const int right = max_pixel_offset - start_pixel_offset;
      bounds = gfx::RectF(left, 0, right - left, location_height);
      break;
    }
    case ax::mojom::WritingDirection::kTtb:
      bounds = gfx::RectF(0, start_pixel_offset, location_width,
                          end_pixel_offset - start_pixel_offset);
      break;
    case ax::mojom::WritingDirection::kBtt: {
      const int top = max_pixel_offset - end_pixel_offset;
      const int bottom = max_pixel_offset - start_pixel_offset;
      bounds = gfx::RectF(0, top, location_width, bottom - top);
      break;
    }
  }

  return bounds;
}

base::string16 BrowserAccessibility::GetValue() const {
  base::string16 value =
      GetString16Attribute(ax::mojom::StringAttribute::kValue);
  // Some screen readers like Jaws and VoiceOver require a value to be set in
  // text fields with rich content, even though the same information is
  // available on the children.
  if (value.empty() && IsRichTextField())
    return BrowserAccessibility::GetInnerText();
  return value;
}

BrowserAccessibility* BrowserAccessibility::ApproximateHitTest(
    const gfx::Point& blink_screen_point) {
  // The best result found that's a child of this object.
  BrowserAccessibility* child_result = nullptr;
  // The best result that's an indirect descendant like grandchild, etc.
  BrowserAccessibility* descendant_result = nullptr;

  // Walk the children recursively looking for the BrowserAccessibility that
  // most tightly encloses the specified point. Walk backwards so that in
  // the absence of any other information, we assume the object that occurs
  // later in the tree is on top of one that comes before it.
  for (int i = static_cast<int>(PlatformChildCount()) - 1; i >= 0; --i) {
    BrowserAccessibility* child = PlatformGetChild(i);

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

bool BrowserAccessibility::HasBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  return GetData().HasBoolAttribute(attribute);
}

bool BrowserAccessibility::GetBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  return GetData().GetBoolAttribute(attribute);
}

bool BrowserAccessibility::GetBoolAttribute(ax::mojom::BoolAttribute attribute,
                                            bool* value) const {
  return GetData().GetBoolAttribute(attribute, value);
}

bool BrowserAccessibility::HasFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  return GetData().HasFloatAttribute(attribute);
}

float BrowserAccessibility::GetFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  return GetData().GetFloatAttribute(attribute);
}

bool BrowserAccessibility::GetFloatAttribute(
    ax::mojom::FloatAttribute attribute,
    float* value) const {
  return GetData().GetFloatAttribute(attribute, value);
}

bool BrowserAccessibility::HasInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  if (GetData().HasStringAttribute(attribute))
    return true;
  return PlatformGetParent() &&
         PlatformGetParent()->HasInheritedStringAttribute(attribute);
}

const std::string& BrowserAccessibility::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  return node_->GetInheritedStringAttribute(attribute);
}

base::string16 BrowserAccessibility::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return node_->GetInheritedString16Attribute(attribute);
}

bool BrowserAccessibility::HasIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  return GetData().HasIntAttribute(attribute);
}

int BrowserAccessibility::GetIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  return GetData().GetIntAttribute(attribute);
}

bool BrowserAccessibility::GetIntAttribute(ax::mojom::IntAttribute attribute,
                                           int* value) const {
  return GetData().GetIntAttribute(attribute, value);
}

bool BrowserAccessibility::HasStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  return GetData().HasStringAttribute(attribute);
}

const std::string& BrowserAccessibility::GetStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  return GetData().GetStringAttribute(attribute);
}

bool BrowserAccessibility::GetStringAttribute(
    ax::mojom::StringAttribute attribute,
    std::string* value) const {
  return GetData().GetStringAttribute(attribute, value);
}

base::string16 BrowserAccessibility::GetString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return GetData().GetString16Attribute(attribute);
}

bool BrowserAccessibility::GetString16Attribute(
    ax::mojom::StringAttribute attribute,
    base::string16* value) const {
  return GetData().GetString16Attribute(attribute, value);
}

bool BrowserAccessibility::HasIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  return GetData().HasIntListAttribute(attribute);
}

const std::vector<int32_t>& BrowserAccessibility::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  return GetData().GetIntListAttribute(attribute);
}

bool BrowserAccessibility::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute,
    std::vector<int32_t>* value) const {
  return GetData().GetIntListAttribute(attribute, value);
}

bool BrowserAccessibility::GetHtmlAttribute(const char* html_attr,
                                            std::string* value) const {
  return GetData().GetHtmlAttribute(html_attr, value);
}

bool BrowserAccessibility::GetHtmlAttribute(const char* html_attr,
                                            base::string16* value) const {
  return GetData().GetHtmlAttribute(html_attr, value);
}

bool BrowserAccessibility::HasState(ax::mojom::State state_enum) const {
  return GetData().HasState(state_enum);
}

bool BrowserAccessibility::HasAction(ax::mojom::Action action_enum) const {
  return GetData().HasAction(action_enum);
}

bool BrowserAccessibility::IsWebAreaForPresentationalIframe() const {
  if (GetRole() != ax::mojom::Role::kWebArea &&
      GetRole() != ax::mojom::Role::kRootWebArea) {
    return false;
  }

  BrowserAccessibility* parent = PlatformGetParent();
  if (!parent)
    return false;

  return parent->GetRole() == ax::mojom::Role::kIframePresentational;
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

bool BrowserAccessibility::IsPlainTextField() const {
  return GetData().IsPlainTextField();
}

bool BrowserAccessibility::IsRichTextField() const {
  return GetData().IsRichTextField();
}

bool BrowserAccessibility::HasExplicitlyEmptyName() const {
  return GetData().GetNameFrom() ==
         ax::mojom::NameFrom::kAttributeExplicitlyEmpty;
}

std::string BrowserAccessibility::GetLiveRegionText() const {
  if (IsIgnored())
    return "";

  std::string text = GetStringAttribute(ax::mojom::StringAttribute::kName);
  if (!text.empty())
    return text;

  for (InternalChildIterator it = InternalChildrenBegin();
       it != InternalChildrenEnd(); ++it) {
    const BrowserAccessibility* child = it.get();
    if (!child)
      continue;

    text += child->GetLiveRegionText();
  }
  return text;
}

std::vector<int> BrowserAccessibility::GetLineStartOffsets() const {
  return node()->GetOrComputeLineStartOffsets();
}

BrowserAccessibilityPosition::AXPositionInstance
BrowserAccessibility::CreatePositionAt(int offset,
                                       ax::mojom::TextAffinity affinity) const {
  DCHECK(manager_);
  return BrowserAccessibilityPosition::CreateTextPosition(
      manager_->ax_tree_id(), GetId(), offset, affinity);
}

// |offset| could either be a text character or a child index in case of
// non-text objects.
// Currently, to be safe, we convert to text leaf equivalents and we don't use
// tree positions.
// TODO(nektar): Remove this function once selection fixes in Blink are
// thoroughly tested and convert to tree positions.
BrowserAccessibilityPosition::AXPositionInstance
BrowserAccessibility::CreatePositionForSelectionAt(int offset) const {
  BrowserAccessibilityPositionInstance position =
      CreatePositionAt(offset, ax::mojom::TextAffinity::kDownstream)
          ->AsLeafTextPosition();
  if (position->GetAnchor() &&
      position->GetAnchor()->GetRole() == ax::mojom::Role::kInlineTextBox) {
    return position->CreateParentPosition();
  }
  return position;
}

base::string16 BrowserAccessibility::GetText() const {
  // Default to inner text for non-native accessibility implementations.
  return GetInnerText();
}

base::string16 BrowserAccessibility::GetNameAsString16() const {
  return base::UTF8ToUTF16(GetName());
}

std::string BrowserAccessibility::GetName() const {
  if (GetRole() == ax::mojom::Role::kPortal &&
      GetData().GetNameFrom() == ax::mojom::NameFrom::kNone) {
    BrowserAccessibility* child_tree_root = PlatformGetRootOfChildTree();
    if (child_tree_root) {
      return child_tree_root->GetStringAttribute(
          ax::mojom::StringAttribute::kName);
    }
  }
  return GetStringAttribute(ax::mojom::StringAttribute::kName);
}

base::string16 BrowserAccessibility::GetHypertext() const {
  // Overloaded by platforms which require a hypertext accessibility text
  // implementation.
  return base::string16();
}

base::string16 BrowserAccessibility::GetInnerText() const {
  return base::UTF8ToUTF16(node()->GetInnerText());
}

gfx::Rect BrowserAccessibility::RelativeToAbsoluteBounds(
    gfx::RectF bounds,
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
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
      ui::AXNode::AXID root_scroller_id =
          manager->GetTreeData().root_scroller_id;
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

    const BrowserAccessibility* root = manager->GetRoot();
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
      BrowserAccessibilityManager* root_manager = manager()->GetRootManager();
      if (root_manager)
        bounds.Scale(root_manager->GetPageScaleFactor());
    }
    bounds.Offset(
        manager()->GetViewBoundsInScreenCoordinates().OffsetFromOrigin());
    if (coordinate_system == ui::AXCoordinateSystem::kScreenPhysicalPixels &&
        !IsUseZoomForDSFEnabled())
      bounds.Scale(manager()->device_scale_factor());
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

bool BrowserAccessibility::IsMinimized() const {
  return false;
}

bool BrowserAccessibility::IsText() const {
  return node()->IsText();
}

bool BrowserAccessibility::IsWebContent() const {
  return true;
}

bool BrowserAccessibility::HasVisibleCaretOrSelection() const {
  ui::AXTree::Selection unignored_selection =
      manager()->ax_tree()->GetUnignoredSelection();
  int32_t focus_id = unignored_selection.focus_object_id;
  BrowserAccessibility* focus_object = manager()->GetFromID(focus_id);
  if (!focus_object)
    return false;

  // Text inputs can have sub-objects that are not exposed, and can cause issues
  // in determining whether a caret is present. Avoid this situation by
  // comparing against the closest platform object, which will be in the tree.
  BrowserAccessibility* platform_object = PlatformGetClosestPlatformObject();
  DCHECK(platform_object);

  // Selection or caret will be visible in a focused editable area, or if caret
  // browsing is enabled.
  // Caret browsing should be looking at leaf text nodes so it might not return
  // expected results in this method. See https://crbug.com/1052091.
  if (platform_object->HasState(ax::mojom::State::kEditable) ||
      BrowserAccessibilityStateImpl::GetInstance()->IsCaretBrowsingEnabled()) {
    return IsPlainTextField() ? focus_object == platform_object
                              : focus_object->IsDescendantOf(platform_object);
  }

  // The selection will be visible in non-editable content only if it is not
  // collapsed into a caret.
  return (focus_id != unignored_selection.anchor_object_id ||
          unignored_selection.focus_offset !=
              unignored_selection.anchor_offset) &&
         focus_object->IsDescendantOf(platform_object);
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
  if (!GetData().GetIntAttribute(attr, &target_id))
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
      if (std::find(nodes.begin(), nodes.end(), node) == nodes.end())
        nodes.push_back(node);
    }
  }

  return nodes;
}

std::set<ui::AXPlatformNode*> BrowserAccessibility::GetReverseRelations(
    ax::mojom::IntAttribute attr) {
  DCHECK(manager_);
  DCHECK(node_);
  DCHECK(ui::IsNodeIdIntAttribute(attr));
  return GetNodesForNodeIdSet(
      manager_->ax_tree()->GetReverseRelations(attr, GetData().id));
}

std::set<ui::AXPlatformNode*> BrowserAccessibility::GetReverseRelations(
    ax::mojom::IntListAttribute attr) {
  DCHECK(manager_);
  DCHECK(node_);
  DCHECK(ui::IsNodeIdIntListAttribute(attr));
  return GetNodesForNodeIdSet(
      manager_->ax_tree()->GetReverseRelations(attr, GetData().id));
}

base::string16 BrowserAccessibility::GetAuthorUniqueId() const {
  base::string16 html_id;
  GetData().GetHtmlAttribute("id", &html_id);
  return html_id;
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

base::Optional<int> BrowserAccessibility::FindTextBoundary(
    ax::mojom::TextBoundary boundary,
    int offset,
    ax::mojom::MoveDirection direction,
    ax::mojom::TextAffinity affinity) const {
  BrowserAccessibilityPositionInstance position =
      CreatePositionAt(offset, affinity);

  // On Windows and Linux ATK, searching for a text boundary should always stop
  // at the boundary of the current object.
  auto boundary_behavior = ui::AXBoundaryBehavior::StopAtAnchorBoundary;
  // On Windows and Linux ATK, it is standard text navigation behavior to stop
  // if we are searching in the backwards direction and the current position is
  // already at the required text boundary.
  DCHECK_NE(direction, ax::mojom::MoveDirection::kNone);
  if (direction == ax::mojom::MoveDirection::kBackward)
    boundary_behavior = ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary;

  return GetBoundaryTextOffsetInsideBaseAnchor(
      direction, position,
      position->CreatePositionAtTextBoundary(boundary, direction,
                                             boundary_behavior));
}

const std::vector<gfx::NativeViewAccessible>
BrowserAccessibility::GetUIADescendants() const {
  // This method is only called on Windows. Other platforms should not call it.
  // The BrowserAccessibilityWin subclass overrides this method.
  NOTREACHED();
  return {};
}

std::string BrowserAccessibility::GetLanguage() const {
  DCHECK(node_) << "Did you forget to call BrowserAccessibility::Init?";
  return node()->GetLanguage();
}

gfx::NativeViewAccessible BrowserAccessibility::GetNativeViewAccessible() {
  // TODO(703369) On Windows, where we have started to migrate to an
  // AXPlatformNode implementation, the BrowserAccessibilityWin subclass has
  // overridden this method. On all other platforms, this method should not be
  // called yet. In the future, when all subclasses have moved over to be
  // implemented by AXPlatformNode, we may make this method completely virtual.
  NOTREACHED();
  return nullptr;
}

//
// AXPlatformNodeDelegate.
//
const ui::AXNodeData& BrowserAccessibility::GetData() const {
  static base::NoDestructor<ui::AXNodeData> empty_data;
  if (node_)
    return node_->data();
  else
    return *empty_data;
}

const ui::AXTreeData& BrowserAccessibility::GetTreeData() const {
  static base::NoDestructor<ui::AXTreeData> empty_data;
  if (manager())
    return manager()->GetTreeData();
  else
    return *empty_data;
}

const ui::AXTree::Selection BrowserAccessibility::GetUnignoredSelection()
    const {
  DCHECK(manager());
  ui::AXTree::Selection selection =
      manager()->ax_tree()->GetUnignoredSelection();

  // "selection.anchor_offset" and "selection.focus_ofset" might need to be
  // adjusted if the anchor or the focus nodes include ignored children.
  const BrowserAccessibility* anchor_object =
      manager()->GetFromID(selection.anchor_object_id);
  if (anchor_object && !anchor_object->PlatformIsLeaf()) {
    DCHECK_GE(selection.anchor_offset, 0);
    if (size_t{selection.anchor_offset} <
        anchor_object->node()->children().size()) {
      const ui::AXNode* anchor_child =
          anchor_object->node()->children()[selection.anchor_offset];
      DCHECK(anchor_child);
      selection.anchor_offset = int{anchor_child->GetUnignoredIndexInParent()};
    } else {
      selection.anchor_offset = anchor_object->GetChildCount();
    }
  }

  const BrowserAccessibility* focus_object =
      manager()->GetFromID(selection.focus_object_id);
  if (focus_object && !focus_object->PlatformIsLeaf()) {
    DCHECK_GE(selection.focus_offset, 0);
    if (size_t{selection.focus_offset} <
        focus_object->node()->children().size()) {
      const ui::AXNode* focus_child =
          focus_object->node()->children()[selection.focus_offset];
      DCHECK(focus_child);
      selection.focus_offset = int{focus_child->GetUnignoredIndexInParent()};
    } else {
      selection.focus_offset = focus_object->GetChildCount();
    }
  }

  return selection;
}

ui::AXNodePosition::AXPositionInstance
BrowserAccessibility::CreateTextPositionAt(int offset) const {
  DCHECK(manager_);
  return ui::AXNodePosition::CreateTextPosition(
      manager_->ax_tree_id(), GetId(), offset,
      ax::mojom::TextAffinity::kDownstream);
}

gfx::NativeViewAccessible BrowserAccessibility::GetNSWindow() {
  NOTREACHED();
  return nullptr;
}

gfx::NativeViewAccessible BrowserAccessibility::GetParent() {
  BrowserAccessibility* parent = PlatformGetParent();
  if (parent)
    return parent->GetNativeViewAccessible();

  BrowserAccessibilityDelegate* delegate =
      manager_->GetDelegateFromRootManager();
  if (!delegate)
    return nullptr;

  return delegate->AccessibilityGetNativeViewAccessible();
}

int BrowserAccessibility::GetChildCount() const {
  return int{PlatformChildCount()};
}

gfx::NativeViewAccessible BrowserAccessibility::ChildAtIndex(int index) {
  BrowserAccessibility* child = PlatformGetChild(index);
  if (!child)
    return nullptr;

  return child->GetNativeViewAccessible();
}

bool BrowserAccessibility::HasModalDialog() const {
  return false;
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

bool BrowserAccessibility::IsChildOfLeaf() const {
  return node()->IsChildOfLeaf();
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
    uint32_t child_count = InternalChildCount();
    return !child_count ||
           (child_count == 1 && InternalGetFirstChild()->IsText());
  }
  return PlatformGetRootOfChildTree() ? false : node()->IsLeaf();
}

bool BrowserAccessibility::IsToplevelBrowserWindow() {
  return false;
}

bool BrowserAccessibility::IsChildOfPlainTextField() const {
  ui::AXNode* textfield_node = node()->GetTextFieldAncestor();
  return textfield_node && textfield_node->data().IsPlainTextField();
}

gfx::NativeViewAccessible BrowserAccessibility::GetClosestPlatformObject()
    const {
  return PlatformGetClosestPlatformObject()->GetNativeViewAccessible();
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

bool BrowserAccessibility::PlatformChildIterator::operator==(
    const ChildIterator& rhs) const {
  return GetIndexInParent() == rhs.GetIndexInParent();
}

bool BrowserAccessibility::PlatformChildIterator::operator!=(
    const ChildIterator& rhs) const {
  return GetIndexInParent() != rhs.GetIndexInParent();
}

void BrowserAccessibility::PlatformChildIterator::operator++() {
  ++platform_iterator;
}

void BrowserAccessibility::PlatformChildIterator::operator++(int) {
  ++platform_iterator;
}

void BrowserAccessibility::PlatformChildIterator::operator--() {
  --platform_iterator;
}

void BrowserAccessibility::PlatformChildIterator::operator--(int) {
  --platform_iterator;
}

BrowserAccessibility* BrowserAccessibility::PlatformChildIterator::get() const {
  return platform_iterator.get();
}

gfx::NativeViewAccessible
BrowserAccessibility::PlatformChildIterator::GetNativeViewAccessible() const {
  return platform_iterator->GetNativeViewAccessible();
}

int BrowserAccessibility::PlatformChildIterator::GetIndexInParent() const {
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

gfx::NativeViewAccessible BrowserAccessibility::GetFocus() {
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

int BrowserAccessibility::GetIndexInParent() {
  if (manager()->GetRoot() == this && PlatformGetParent() == nullptr) {
    // If it is a root node of WebContent, it doesn't have a parent and a
    // valid index in parent. So it returns -1 in order to compute its
    // index at AXPlatformNodeBase.
    return -1;
  }
  return node()->GetUnignoredIndexInParent();
}

gfx::AcceleratedWidget
BrowserAccessibility::GetTargetForNativeAccessibilityEvent() {
  BrowserAccessibilityDelegate* root_delegate =
      manager()->GetDelegateFromRootManager();
  if (!root_delegate)
    return gfx::kNullAcceleratedWidget;
  return root_delegate->AccessibilityGetAcceleratedWidget();
}

bool BrowserAccessibility::IsTable() const {
  return node()->IsTable();
}

base::Optional<int> BrowserAccessibility::GetTableRowCount() const {
  return node()->GetTableRowCount();
}

base::Optional<int> BrowserAccessibility::GetTableColCount() const {
  return node()->GetTableColCount();
}

base::Optional<int> BrowserAccessibility::GetTableAriaColCount() const {
  return node()->GetTableAriaColCount();
}

base::Optional<int> BrowserAccessibility::GetTableAriaRowCount() const {
  return node()->GetTableAriaRowCount();
}

base::Optional<int> BrowserAccessibility::GetTableCellCount() const {
  return node()->GetTableCellCount();
}

base::Optional<bool> BrowserAccessibility::GetTableHasColumnOrRowHeaderNode()
    const {
  return node()->GetTableHasColumnOrRowHeaderNode();
}

std::vector<ui::AXNode::AXID> BrowserAccessibility::GetColHeaderNodeIds()
    const {
  return node()->GetTableColHeaderNodeIds();
}

std::vector<ui::AXNode::AXID> BrowserAccessibility::GetColHeaderNodeIds(
    int col_index) const {
  return node()->GetTableColHeaderNodeIds(col_index);
}

std::vector<ui::AXNode::AXID> BrowserAccessibility::GetRowHeaderNodeIds()
    const {
  return node()->GetTableCellRowHeaderNodeIds();
}

std::vector<ui::AXNode::AXID> BrowserAccessibility::GetRowHeaderNodeIds(
    int row_index) const {
  return node()->GetTableRowHeaderNodeIds(row_index);
}

ui::AXPlatformNode* BrowserAccessibility::GetTableCaption() const {
  ui::AXNode* caption = node()->GetTableCaption();
  if (caption)
    return const_cast<BrowserAccessibility*>(this)->GetFromNodeID(
        caption->id());

  return nullptr;
}

bool BrowserAccessibility::IsTableRow() const {
  return node()->IsTableRow();
}

base::Optional<int> BrowserAccessibility::GetTableRowRowIndex() const {
  return node()->GetTableRowRowIndex();
}

bool BrowserAccessibility::IsTableCellOrHeader() const {
  return node()->IsTableCellOrHeader();
}

base::Optional<int> BrowserAccessibility::GetTableCellColIndex() const {
  return node()->GetTableCellColIndex();
}

base::Optional<int> BrowserAccessibility::GetTableCellRowIndex() const {
  return node()->GetTableCellRowIndex();
}

base::Optional<int> BrowserAccessibility::GetTableCellColSpan() const {
  return node()->GetTableCellColSpan();
}

base::Optional<int> BrowserAccessibility::GetTableCellRowSpan() const {
  return node()->GetTableCellRowSpan();
}

base::Optional<int> BrowserAccessibility::GetTableCellAriaColIndex() const {
  return node()->GetTableCellAriaColIndex();
}

base::Optional<int> BrowserAccessibility::GetTableCellAriaRowIndex() const {
  return node()->GetTableCellAriaRowIndex();
}

base::Optional<int32_t> BrowserAccessibility::GetCellId(int row_index,
                                                        int col_index) const {
  ui::AXNode* cell = node()->GetTableCellFromCoords(row_index, col_index);
  if (!cell)
    return base::nullopt;
  return cell->id();
}

base::Optional<int> BrowserAccessibility::GetTableCellIndex() const {
  return node()->GetTableCellIndex();
}

base::Optional<int32_t> BrowserAccessibility::CellIndexToId(
    int cell_index) const {
  ui::AXNode* cell = node()->GetTableCellFromIndex(cell_index);
  if (!cell)
    return base::nullopt;
  return cell->id();
}

bool BrowserAccessibility::IsCellOrHeaderOfARIATable() const {
  return node()->IsCellOrHeaderOfARIATable();
}

bool BrowserAccessibility::IsCellOrHeaderOfARIAGrid() const {
  return node()->IsCellOrHeaderOfARIAGrid();
}

bool BrowserAccessibility::AccessibilityPerformAction(
    const ui::AXActionData& data) {
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
          data.target_point - manager_->GetRoot()
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
      // "data.anchor_offset" and "data.focus_ofset" might need to be adjusted
      // if the anchor or the focus nodes include ignored children.
      ui::AXActionData selection = data;
      const BrowserAccessibility* anchor_object =
          manager()->GetFromID(selection.anchor_node_id);
      DCHECK(anchor_object);
      if (!anchor_object->PlatformIsLeaf()) {
        DCHECK_GE(selection.anchor_offset, 0);
        const BrowserAccessibility* anchor_child =
            anchor_object->InternalGetChild(uint32_t{selection.anchor_offset});
        if (anchor_child) {
          selection.anchor_offset =
              int{anchor_child->node()->index_in_parent()};
          selection.anchor_node_id = anchor_child->node()->parent()->id();
        } else {
          // Since the child was not found, the only alternative is that this is
          // an "after children" position.
          selection.anchor_offset =
              int{anchor_object->node()->children().size()};
        }
      }

      const BrowserAccessibility* focus_object =
          manager()->GetFromID(selection.focus_node_id);
      DCHECK(focus_object);
      if (!focus_object->PlatformIsLeaf()) {
        DCHECK_GE(selection.focus_offset, 0);
        const BrowserAccessibility* focus_child =
            focus_object->InternalGetChild(uint32_t{selection.focus_offset});
        if (focus_child) {
          selection.focus_offset = int{focus_child->node()->index_in_parent()};
          selection.focus_node_id = focus_child->node()->parent()->id();
        } else {
          // Since the child was not found, the only alternative is that this is
          // an "after children" position.
          selection.focus_offset = int{focus_object->node()->children().size()};
        }
      }

      manager_->SetSelection(selection);
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
    default:
      return false;
  }
}

base::string16 BrowserAccessibility::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
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
      return base::string16();
  }

  DCHECK(message_id);

  return content_client->GetLocalizedString(message_id);
}

base::string16
BrowserAccessibility::GetLocalizedRoleDescriptionForUnlabeledImage() const {
  ContentClient* content_client = content::GetContentClient();
  return content_client->GetLocalizedString(
      IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION);
}

base::string16 BrowserAccessibility::GetLocalizedStringForLandmarkType() const {
  ContentClient* content_client = content::GetContentClient();
  const ui::AXNodeData& data = GetData();

  switch (data.role) {
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kHeader:
      return content_client->GetLocalizedString(IDS_AX_ROLE_BANNER);

    case ax::mojom::Role::kComplementary:
      return content_client->GetLocalizedString(IDS_AX_ROLE_COMPLEMENTARY);

    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
      return content_client->GetLocalizedString(IDS_AX_ROLE_CONTENT_INFO);

    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kSection:
      if (data.HasStringAttribute(ax::mojom::StringAttribute::kName))
        return content_client->GetLocalizedString(IDS_AX_ROLE_REGION);
      FALLTHROUGH;

    default:
      return {};
  }
}

base::string16 BrowserAccessibility::GetLocalizedStringForRoleDescription()
    const {
  ContentClient* content_client = content::GetContentClient();
  const ui::AXNodeData& data = GetData();

  switch (data.role) {
    case ax::mojom::Role::kArticle:
      return content_client->GetLocalizedString(IDS_AX_ROLE_ARTICLE);

    case ax::mojom::Role::kAudio:
      return content_client->GetLocalizedString(IDS_AX_ROLE_AUDIO);

    case ax::mojom::Role::kCode:
      return content_client->GetLocalizedString(IDS_AX_ROLE_CODE);

    case ax::mojom::Role::kColorWell:
      return content_client->GetLocalizedString(IDS_AX_ROLE_COLOR_WELL);

    case ax::mojom::Role::kContentInfo:
      return content_client->GetLocalizedString(IDS_AX_ROLE_CONTENT_INFO);

    case ax::mojom::Role::kDate:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DATE);

    case ax::mojom::Role::kDateTime: {
      std::string input_type;
      if (data.GetStringAttribute(ax::mojom::StringAttribute::kInputType,
                                  &input_type)) {
        if (input_type == "datetime-local") {
          return content_client->GetLocalizedString(
              IDS_AX_ROLE_DATE_TIME_LOCAL);
        } else if (input_type == "week") {
          return content_client->GetLocalizedString(IDS_AX_ROLE_WEEK);
        }
      }
      return {};
    }

    case ax::mojom::Role::kDetails:
      return content_client->GetLocalizedString(IDS_AX_ROLE_DETAILS);

    case ax::mojom::Role::kEmphasis:
      return content_client->GetLocalizedString(IDS_AX_ROLE_EMPHASIS);

    case ax::mojom::Role::kFigure:
      return content_client->GetLocalizedString(IDS_AX_ROLE_FIGURE);

    case ax::mojom::Role::kFooter:
    case ax::mojom::Role::kFooterAsNonLandmark:
      return content_client->GetLocalizedString(IDS_AX_ROLE_FOOTER);

    case ax::mojom::Role::kHeader:
    case ax::mojom::Role::kHeaderAsNonLandmark:
      return content_client->GetLocalizedString(IDS_AX_ROLE_HEADER);

    case ax::mojom::Role::kMark:
      return content_client->GetLocalizedString(IDS_AX_ROLE_MARK);

    case ax::mojom::Role::kMeter:
      return content_client->GetLocalizedString(IDS_AX_ROLE_METER);

    case ax::mojom::Role::kSearchBox:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SEARCH_BOX);

    case ax::mojom::Role::kSection: {
      if (data.HasStringAttribute(ax::mojom::StringAttribute::kName))
        return content_client->GetLocalizedString(IDS_AX_ROLE_SECTION);

      return {};
    }

    case ax::mojom::Role::kStatus:
      return content_client->GetLocalizedString(IDS_AX_ROLE_OUTPUT);

    case ax::mojom::Role::kStrong:
      return content_client->GetLocalizedString(IDS_AX_ROLE_STRONG);

    case ax::mojom::Role::kSwitch:
      return content_client->GetLocalizedString(IDS_AX_ROLE_SWITCH);

    case ax::mojom::Role::kTextField: {
      std::string input_type;
      if (data.GetStringAttribute(ax::mojom::StringAttribute::kInputType,
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

    default:
      return {};
  }
}

base::string16 BrowserAccessibility::GetStyleNameAttributeAsLocalizedString()
    const {
  const BrowserAccessibility* current_node = this;
  while (current_node) {
    if (current_node->GetData().role == ax::mojom::Role::kMark) {
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

bool BrowserAccessibility::IsOrderedSetItem() const {
  return node()->IsOrderedSetItem();
}

bool BrowserAccessibility::IsOrderedSet() const {
  return node()->IsOrderedSet();
}

base::Optional<int> BrowserAccessibility::GetPosInSet() const {
  return node()->GetPosInSet();
}

base::Optional<int> BrowserAccessibility::GetSetSize() const {
  return node()->GetSetSize();
}

bool BrowserAccessibility::IsInListMarker() const {
  return node()->IsInListMarker();
}

bool BrowserAccessibility::IsCollapsedMenuListPopUpButton() const {
  return node()->IsCollapsedMenuListPopUpButton();
}

BrowserAccessibility*
BrowserAccessibility::GetCollapsedMenuListPopUpButtonAncestor() const {
  ui::AXNode* popup_button = node()->GetCollapsedMenuListPopUpButtonAncestor();
  if (!popup_button)
    return nullptr;

  return manager()->GetFromAXNode(popup_button);
}

std::string BrowserAccessibility::ToString() const {
  return GetData().ToString();
}

bool BrowserAccessibility::SetHypertextSelection(int start_offset,
                                                 int end_offset) {
  manager()->SetSelection(
      AXPlatformRange(CreatePositionForSelectionAt(start_offset),
                      CreatePositionForSelectionAt(end_offset)));
  return true;
}

BrowserAccessibility* BrowserAccessibility::PlatformGetRootOfChildTree() const {
  std::string child_tree_id;
  if (!GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId,
                          &child_tree_id)) {
    return nullptr;
  }
  DCHECK_EQ(node_->children().size(), 0u)
      << "A node should not have both children and a child tree.";

  BrowserAccessibilityManager* child_manager =
      BrowserAccessibilityManager::FromID(AXTreeID::FromString(child_tree_id));
  if (child_manager && child_manager->GetRoot()->PlatformGetParent() == this)
    return child_manager->GetRoot();
  return nullptr;
}

ui::TextAttributeList BrowserAccessibility::ComputeTextAttributes() const {
  return ui::TextAttributeList();
}

std::string BrowserAccessibility::GetInheritedFontFamilyName() const {
  return GetInheritedStringAttribute(ax::mojom::StringAttribute::kFontFamily);
}

ui::TextAttributeMap BrowserAccessibility::GetSpellingAndGrammarAttributes()
    const {
  ui::TextAttributeMap spelling_attributes;
  if (IsText()) {
    const std::vector<int32_t>& marker_types =
        GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
    const std::vector<int>& marker_starts =
        GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts);
    const std::vector<int>& marker_ends =
        GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);
    for (size_t i = 0; i < marker_types.size(); ++i) {
      bool is_spelling_error =
          (marker_types[i] &
           static_cast<int32_t>(ax::mojom::MarkerType::kSpelling)) != 0;
      bool is_grammar_error =
          (marker_types[i] &
           static_cast<int32_t>(ax::mojom::MarkerType::kGrammar)) != 0;

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

  if (IsPlainTextField()) {
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
  ui::TextAttributeMap attributes_map;
  if (PlatformIsLeaf() || IsPlainTextField()) {
    attributes_map[0] = default_attributes;
    const ui::TextAttributeMap spelling_attributes =
        GetSpellingAndGrammarAttributes();
    MergeSpellingAndGrammarIntoTextAttributes(
        spelling_attributes, 0 /* start_offset */, &attributes_map);
    return attributes_map;
  }

  DCHECK(PlatformChildCount());

  int start_offset = 0;
  for (BrowserAccessibility::PlatformChildIterator it = PlatformChildrenBegin();
       it != PlatformChildrenEnd(); ++it) {
    BrowserAccessibility* child = it.get();
    DCHECK(child);
    ui::TextAttributeList attributes(child->ComputeTextAttributes());

    if (attributes_map.empty()) {
      attributes_map[start_offset] = attributes;
    } else {
      // Only add the attributes for this child if we are at the start of a new
      // style span.
      ui::TextAttributeList previous_attributes =
          attributes_map.rbegin()->second;
      // Must check the size, otherwise if attributes is a subset of
      // prev_attributes, they would appear to be equal.
      if (attributes.size() != previous_attributes.size() ||
          !std::equal(attributes.begin(), attributes.end(),
                      previous_attributes.begin())) {
        attributes_map[start_offset] = attributes;
      }
    }

    if (child->IsText()) {
      const ui::TextAttributeMap spelling_attributes =
          child->GetSpellingAndGrammarAttributes();
      MergeSpellingAndGrammarIntoTextAttributes(spelling_attributes,
                                                start_offset, &attributes_map);
      start_offset += child->GetHypertext().length();
    } else {
      start_offset += 1;
    }
  }

  return attributes_map;
}

// static
bool BrowserAccessibility::HasInvalidAttribute(
    const ui::TextAttributeList& attributes) {
  return std::find_if(attributes.begin(), attributes.end(),
                      [](const ui::TextAttribute& attribute) {
                        return attribute.first == "invalid";
                      }) != attributes.end();
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
  if (!ui::IsStaticList(GetRole()))
    return false;
  return HasListDescendant(this, this) || HasListAncestor(InternalGetParent());
}

}  // namespace content
