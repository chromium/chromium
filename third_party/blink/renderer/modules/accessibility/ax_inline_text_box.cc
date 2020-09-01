/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"

#include <utility>

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_api_shim.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"

namespace blink {

AXInlineTextBox::AXInlineTextBox(
    scoped_refptr<AbstractInlineTextBox> inline_text_box,
    AXObjectCacheImpl& ax_object_cache)
    : AXObject(ax_object_cache), inline_text_box_(std::move(inline_text_box)) {}

ax::mojom::blink::Role AXInlineTextBox::RoleValue() const {
  return ax::mojom::blink::Role::kInlineTextBox;
}

void AXInlineTextBox::GetRelativeBounds(AXObject** out_container,
                                        FloatRect& out_bounds_in_container,
                                        SkMatrix44& out_container_transform,
                                        bool* clips_children) const {
  *out_container = nullptr;
  out_bounds_in_container = FloatRect();
  out_container_transform.setIdentity();

  if (!inline_text_box_ || !ParentObject() ||
      !ParentObject()->GetLayoutObject()) {
    return;
  }

  *out_container = ParentObject();
  out_bounds_in_container = FloatRect(inline_text_box_->LocalBounds());

  // Subtract the local bounding box of the parent because they're
  // both in the same coordinate system.
  FloatRect parent_bounding_box =
      ParentObject()->LocalBoundingBoxRectForAccessibility();
  out_bounds_in_container.MoveBy(-parent_bounding_box.Location());
}

bool AXInlineTextBox::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  AXObject* parent = ParentObject();
  if (!parent)
    return false;

  if (!parent->AccessibilityIsIgnored())
    return false;

  if (ignored_reasons)
    parent->ComputeAccessibilityIsIgnored(ignored_reasons);

  return true;
}

void AXInlineTextBox::TextCharacterOffsets(Vector<int>& offsets) const {
  if (IsDetached())
    return;

  Vector<float> widths;
  inline_text_box_->CharacterWidths(widths);
  DCHECK_EQ(int{widths.size()}, TextLength());
  offsets.resize(TextLength());

  float width_so_far = 0;
  for (int i = 0; i < TextLength(); i++) {
    width_so_far += widths[i];
    offsets[i] = roundf(width_so_far);
  }
}

void AXInlineTextBox::GetWordBoundaries(Vector<int>& word_starts,
                                        Vector<int>& word_ends) const {
  if (!inline_text_box_ ||
      inline_text_box_->GetText().ContainsOnlyWhitespaceOrEmpty()) {
    return;
  }

  Vector<AbstractInlineTextBox::WordBoundaries> boundaries;
  inline_text_box_->GetWordBoundaries(boundaries);
  word_starts.ReserveCapacity(boundaries.size());
  word_ends.ReserveCapacity(boundaries.size());
  for (const auto& boundary : boundaries) {
    word_starts.push_back(boundary.start_index);
    word_ends.push_back(boundary.end_index);
  }
}

int AXInlineTextBox::TextOffsetInFormattingContext(int offset) const {
  DCHECK_GE(offset, 0);
  if (IsDetached())
    return 0;

  // Retrieve the text offset from the start of the layout block flow ancestor.
  return int{inline_text_box_->TextOffsetInFormattingContext(
      static_cast<unsigned int>(offset))};
}

int AXInlineTextBox::TextOffsetInContainer(int offset) const {
  DCHECK_GE(offset, 0);
  if (IsDetached())
    return 0;

  // Retrieve the text offset from the start of the layout block flow ancestor.
  int offset_in_block_flow_container = TextOffsetInFormattingContext(offset);
  const AXObject* parent = ParentObject();
  if (!parent)
    return offset_in_block_flow_container;

  // If the parent object in the accessibility tree exists, then it is either
  // a static text object or a line break. In the static text case, it is an
  // AXLayoutObject associated with an inline text object. Hence the container
  // is another inline object, not a layout block flow. We need to subtract the
  // text start offset of the static text parent from the text start offset of
  // this inline text box.
  int offset_in_inline_parent = parent->TextOffsetInFormattingContext(0);
  DCHECK_LE(offset_in_inline_parent, offset_in_block_flow_container);
  return offset_in_block_flow_container - offset_in_inline_parent;
}

String AXInlineTextBox::GetName(ax::mojom::blink::NameFrom& name_from,
                                AXObject::AXObjectVector* name_objects) const {
  if (IsDetached())
    return String();

  name_from = ax::mojom::blink::NameFrom::kContents;
  return inline_text_box_->GetText();
}

AXObject* AXInlineTextBox::ComputeParent() const {
  DCHECK(!IsDetached());
  if (!inline_text_box_ || !ax_object_cache_)
    return nullptr;
  LineLayoutText line_layout_text = inline_text_box_->GetLineLayoutItem();
  if (!line_layout_text)
    return nullptr;
  return ax_object_cache_->GetOrCreate(
      LineLayoutAPIShim::LayoutObjectFrom(line_layout_text));
}

// In addition to LTR and RTL direction, edit fields also support
// top to bottom and bottom to top via the CSS writing-mode property.
ax::mojom::blink::WritingDirection AXInlineTextBox::GetTextDirection() const {
  if (IsDetached())
    return AXObject::GetTextDirection();

  switch (inline_text_box_->GetDirection()) {
    case AbstractInlineTextBox::kLeftToRight:
      return ax::mojom::blink::WritingDirection::kLtr;
    case AbstractInlineTextBox::kRightToLeft:
      return ax::mojom::blink::WritingDirection::kRtl;
    case AbstractInlineTextBox::kTopToBottom:
      return ax::mojom::blink::WritingDirection::kTtb;
    case AbstractInlineTextBox::kBottomToTop:
      return ax::mojom::blink::WritingDirection::kBtt;
  }

  return AXObject::GetTextDirection();
}

Node* AXInlineTextBox::GetNode() const {
  if (IsDetached())
    return nullptr;

  return inline_text_box_->GetNode();
}

AXObject* AXInlineTextBox::NextOnLine() const {
  if (IsDetached())
    return nullptr;

  if (inline_text_box_->IsLast())
    return ParentObject()->NextOnLine();

  scoped_refptr<AbstractInlineTextBox> next_on_line =
      inline_text_box_->NextOnLine();
  if (next_on_line)
    return ax_object_cache_->GetOrCreate(next_on_line.get());

  return nullptr;
}

AXObject* AXInlineTextBox::PreviousOnLine() const {
  if (IsDetached())
    return nullptr;

  if (inline_text_box_->IsFirst())
    return ParentObject()->PreviousOnLine();

  scoped_refptr<AbstractInlineTextBox> previous_on_line =
      inline_text_box_->PreviousOnLine();
  if (previous_on_line)
    return ax_object_cache_->GetOrCreate(previous_on_line.get());

  return nullptr;
}

void AXInlineTextBox::Init() {}

void AXInlineTextBox::Detach() {
  inline_text_box_ = nullptr;
  AXObject::Detach();
}

bool AXInlineTextBox::IsDetached() const {
  return !inline_text_box_ || AXObject::IsDetached();
}

bool AXInlineTextBox::IsAXInlineTextBox() const {
  return true;
}

bool AXInlineTextBox::IsLineBreakingObject() const {
  if (IsDetached())
    return AXObject::IsLineBreakingObject();

  // If this object is a forced line break, or the parent is a <br>
  // element, then this object is line breaking.
  const AXObject* parent = ParentObject();
  return inline_text_box_->IsLineBreak() ||
         (parent && parent->RoleValue() == ax::mojom::blink::Role::kLineBreak);
}

int AXInlineTextBox::TextLength() const {
  if (IsDetached())
    return 0;
  return int{inline_text_box_->Len()};
}

}  // namespace blink
