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

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/numerics/clamped_math.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/highlight/highlight.h"
#include "third_party/blink/renderer/core/layout/inline/abstract_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

AXInlineTextBox::AXInlineTextBox(AbstractInlineTextBox* inline_text_box,
                                 AXObjectCacheImpl& ax_object_cache)
    : AXObject(ax_object_cache), inline_text_box_(inline_text_box) {}

void AXInlineTextBox::Trace(Visitor* visitor) const {
  visitor->Trace(inline_text_box_);
  AXObject::Trace(visitor);
}

void AXInlineTextBox::GetRelativeBounds(AXObject** out_container,
                                        gfx::RectF& out_bounds_in_container,
                                        gfx::Transform& out_container_transform,
                                        bool* clips_children) const {
  *out_container = nullptr;
  out_bounds_in_container = gfx::RectF();
  out_container_transform.MakeIdentity();

  if (!inline_text_box_ || !ParentObject() ||
      !ParentObject()->GetLayoutObject()) {
    return;
  }

  *out_container = ParentObject();
  out_bounds_in_container = gfx::RectF(inline_text_box_->LocalBounds());

  // Subtract the local bounding box of the parent because they're
  // both in the same coordinate system.
  gfx::RectF parent_bounding_box =
      ParentObject()->LocalBoundingBoxRectForAccessibility();
  out_bounds_in_container.Offset(-parent_bounding_box.OffsetFromOrigin());
}

bool AXInlineTextBox::ComputeIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  AXObject* parent = ParentObject();
  if (!parent)
    return false;

  if (!parent->IsIgnored())
    return false;

  if (ignored_reasons)
    parent->ComputeIsIgnored(ignored_reasons);

  return true;
}

void AXInlineTextBox::TextCharacterOffsets(Vector<int>& offsets) const {
  if (IsDetached())
    return;

  Vector<float> widths;
  inline_text_box_->CharacterWidths(widths);
  DCHECK_EQ(static_cast<int>(widths.size()), TextLength());
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
  word_starts.reserve(boundaries.size());
  word_ends.reserve(boundaries.size());
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
  return static_cast<int>(inline_text_box_->TextOffsetInFormattingContext(
      static_cast<unsigned int>(offset)));
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
  // AXNodeObject associated with an inline text object. Hence the container
  // is another inline object, not a layout block flow. We need to subtract the
  // text start offset of the static text parent from the text start offset of
  // this inline text box.
  int offset_in_inline_parent = parent->TextOffsetInFormattingContext(0);
  // TODO(nektar) Figure out why this asserts in marker-hyphens.html.
  // To see error, comment out below early return and run command similar to:
  // run_web_tests.py --driver-logging -t linux-debug
  //   --additional-driver-flag=--force-renderer-accessibility
  //   external/wpt/css/css-pseudo/marker-hyphens.html
  // DCHECK_LE(offset_in_inline_parent, offset_in_block_flow_container);
  return offset_in_block_flow_container - offset_in_inline_parent;
}

String AXInlineTextBox::GetName(ax::mojom::blink::NameFrom& name_from,
                                AXObject::AXObjectVector* name_objects) const {
  if (IsDetached())
    return String();

  name_from = ax::mojom::blink::NameFrom::kContents;
  return inline_text_box_->GetText();
}

// In addition to LTR and RTL direction, edit fields also support
// top to bottom and bottom to top via the CSS writing-mode property.
ax::mojom::blink::WritingDirection AXInlineTextBox::GetTextDirection() const {
  if (IsDetached())
    return AXObject::GetTextDirection();

  switch (inline_text_box_->GetDirection()) {
    case PhysicalDirection::kRight:
      return ax::mojom::blink::WritingDirection::kLtr;
    case PhysicalDirection::kLeft:
      return ax::mojom::blink::WritingDirection::kRtl;
    case PhysicalDirection::kDown:
      return ax::mojom::blink::WritingDirection::kTtb;
    case PhysicalDirection::kUp:
      return ax::mojom::blink::WritingDirection::kBtt;
  }

  return AXObject::GetTextDirection();
}

Document* AXInlineTextBox::GetDocument() const {
  return ParentObject() ? ParentObject()->GetDocument() : nullptr;
}

AbstractInlineTextBox* AXInlineTextBox::GetInlineTextBox() const {
  return inline_text_box_.Get();
}

AXObject* AXInlineTextBox::NextOnLine() const {
  if (IsDetached())
    return nullptr;

  if (inline_text_box_->IsLast()) {
    // Do not serialize nextOnlineID if it can be inferred from the parent.
    return features::IsAccessibilityPruneRedundantInlineConnectivityEnabled()
               ? nullptr
               : ParentObject()->NextOnLine();
  }

  if (AbstractInlineTextBox* next_on_line = inline_text_box_->NextOnLine()) {
    return AXObjectCache().Get(next_on_line);
  }
  return nullptr;
}

AXObject* AXInlineTextBox::PreviousOnLine() const {
  if (IsDetached())
    return nullptr;

  if (inline_text_box_->IsFirst()) {
    // Do not serialize previousOnlineID if it can be inferred from the parent.
    return features::IsAccessibilityPruneRedundantInlineConnectivityEnabled()
               ? nullptr
               : ParentObject()->PreviousOnLine();
  }

  AbstractInlineTextBox* previous_on_line = inline_text_box_->PreviousOnLine();
  if (previous_on_line)
    return AXObjectCache().Get(previous_on_line);

  return nullptr;
}

void AXInlineTextBox::SerializeMarkerAttributes(
    ui::AXNodeData* node_data) const {
  // TODO(nektar) Address 20% performance degredation and restore code.
  // It may be necessary to add document markers as part of tree data instead
  // of computing for every node. To measure current performance, create a
  // release build without DCHECKs, and then run command similar to:
  // tools/perf/run_benchmark blink_perf.accessibility   --browser=exact \
  //   --browser-executable=path/to/chrome --story-filter="accessibility.*"
  //   --results-label="[my-branch-name]"
  // Pay attention only to rows with  ProcessDeferredAccessibilityEvents
  // and RenderAccessibilityImpl::SendPendingAccessibilityEvents.
  if (!RuntimeEnabledFeatures::
          AccessibilityUseAXPositionForDocumentMarkersEnabled())
    return;

  if (IsDetached())
    return;
  if (!GetDocument() || GetDocument()->IsSlotAssignmentDirty()) {
    // In order to retrieve the document markers we need access to the flat
    // tree. If the slot assignments in a shadow DOM subtree are dirty,
    // accessing the flat tree will cause them to be updated, which could in
    // turn cause an update to the accessibility tree, potentially causing this
    // method to be called repeatedly.
    return;  // Wait until distribution for flat tree traversal has been
             // updated.
  }

  int text_length = TextLength();
  if (!text_length)
    return;
  const auto ax_range = AXRange::RangeOfContents(*this);

  std::vector<int32_t> marker_types;
  std::vector<int32_t> highlight_types;
  std::vector<int32_t> marker_starts;
  std::vector<int32_t> marker_ends;

  // First use ARIA markers for spelling/grammar if available.
  std::optional<DocumentMarker::MarkerType> aria_marker_type =
      GetAriaSpellingOrGrammarMarker();
  if (aria_marker_type) {
    marker_types.push_back(ToAXMarkerType(aria_marker_type.value()));
    marker_starts.push_back(ax_range.Start().TextOffset());
    marker_ends.push_back(ax_range.End().TextOffset());
  }

  DocumentMarkerController& marker_controller = GetDocument()->Markers();
  const Position dom_range_start =
      ax_range.Start().ToPosition(AXPositionAdjustmentBehavior::kMoveLeft);
  const Position dom_range_end =
      ax_range.End().ToPosition(AXPositionAdjustmentBehavior::kMoveRight);
  if (dom_range_start.IsNull() || dom_range_end.IsNull())
    return;

  // TODO(nektar) Figure out why the start > end sometimes.
  // To see error, comment out below early return and run command similar to:
  // run_web_tests.py --driver-logging -t linux-debug
  //   --additional-driver-flag=--force-renderer-accessibility
  //   external/wpt/css/css-ui/text-overflow-006.html
  if (dom_range_start > dom_range_end)
    return;  // Temporary until above TODO is resolved.
  DCHECK_LE(dom_range_start, dom_range_end);
  const EphemeralRangeInFlatTree dom_range(
      ToPositionInFlatTree(dom_range_start),
      ToPositionInFlatTree(dom_range_end));
  DCHECK(dom_range.IsNotNull());
  const DocumentMarker::MarkerTypes markers_used_by_accessibility(
      DocumentMarker::kSpelling | DocumentMarker::kGrammar |
      DocumentMarker::kTextMatch | DocumentMarker::kActiveSuggestion |
      DocumentMarker::kSuggestion | DocumentMarker::kTextFragment |
      DocumentMarker::kCustomHighlight);
  // "MarkersIntersectingRange" performs a binary search through the document
  // markers list for markers in the given range and of the given types. It
  // should be of a logarithmic complexity.
  const VectorOfPairs<const Text, DocumentMarker> node_marker_pairs =
      marker_controller.MarkersIntersectingRange(dom_range,
                                                 markers_used_by_accessibility);
  const int start_text_offset_in_parent = TextOffsetInContainer(0);
  for (const auto& node_marker_pair : node_marker_pairs) {
    DCHECK_EQ(inline_text_box_->GetNode(), node_marker_pair.first);
    const DocumentMarker* marker = node_marker_pair.second;

    if (aria_marker_type == marker->GetType())
      continue;

    // The document markers are represented by DOM offsets in this object's
    // static text parent. We need to translate to text offsets in the
    // accessibility tree, first in this object's parent and then to local text
    // offsets.
    const auto start_position = AXPosition::FromPosition(
        Position(*inline_text_box_->GetNode(), marker->StartOffset()),
        TextAffinity::kDownstream, AXPositionAdjustmentBehavior::kMoveLeft);
    const auto end_position = AXPosition::FromPosition(
        Position(*inline_text_box_->GetNode(), marker->EndOffset()),
        TextAffinity::kDownstream, AXPositionAdjustmentBehavior::kMoveRight);
    if (!start_position.IsValid() || !end_position.IsValid())
      continue;

    const int local_start_offset = base::ClampMax(
        start_position.TextOffset() - start_text_offset_in_parent, 0);
    DCHECK_LE(local_start_offset, text_length);
    const int local_end_offset = base::ClampMin(
        end_position.TextOffset() - start_text_offset_in_parent, text_length);
    DCHECK_GE(local_end_offset, 0);

    int32_t highlight_type =
        static_cast<int32_t>(ax::mojom::blink::HighlightType::kNone);
    if (marker->GetType() == DocumentMarker::kCustomHighlight) {
      const auto& highlight_marker = To<CustomHighlightMarker>(*marker);
      highlight_type =
          ToAXHighlightType(highlight_marker.GetHighlight()->type());
    }

    marker_types.push_back(int32_t{ToAXMarkerType(marker->GetType())});
    highlight_types.push_back(static_cast<int32_t>(highlight_type));
    marker_starts.push_back(local_start_offset);
    marker_ends.push_back(local_end_offset);
  }

  DCHECK_EQ(marker_types.size(), marker_starts.size());
  DCHECK_EQ(marker_types.size(), marker_ends.size());

  if (marker_types.empty())
    return;

  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kMarkerTypes, marker_types);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kHighlightTypes, highlight_types);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kMarkerStarts, marker_starts);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kMarkerEnds, marker_ends);
}

void AXInlineTextBox::Init(AXObject* parent) {
  CHECK(!AXObjectCache().IsFrozen());
  role_ = ax::mojom::blink::Role::kInlineTextBox;
  DCHECK(parent);
  DCHECK(ui::CanHaveInlineTextBoxChildren(parent->RoleValue()))
      << "Unexpected parent of inline text box: " << parent->RoleValue();
  DCHECK(parent->CanHaveChildren())
      << "Parent cannot have children: " << parent;
  // Don't call SetParent(), which calls SetAncestorsHaveDirtyDescendants(),
  // because once inline textboxes are loaded for the parent text, it's never
  // necessary to again recompute this part of the tree.
  parent_ = parent;
  UpdateCachedAttributeValuesIfNeeded(false);
}

void AXInlineTextBox::Detach() {
  AXObject::Detach();
  inline_text_box_ = nullptr;
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
  return static_cast<int>(inline_text_box_->Len());
}

void AXInlineTextBox::ClearChildren() {
  // An AXInlineTextBox has no children to clear.
}

}  // namespace blink
