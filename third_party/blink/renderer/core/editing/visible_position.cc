/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/visible_position.h"

#include <ostream>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

template <typename Strategy>
VisiblePositionTemplate<Strategy>::VisiblePositionTemplate()
#if DCHECK_IS_ON()
    : dom_tree_version_(0),
      style_version_(0)
#endif
{
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>::VisiblePositionTemplate(
    const PositionWithAffinityTemplate<Strategy>& position_with_affinity)
    : position_with_affinity_(position_with_affinity)
#if DCHECK_IS_ON()
      ,
      dom_tree_version_(position_with_affinity.GetDocument()->DomTreeVersion()),
      style_version_(position_with_affinity.GetDocument()->StyleVersion())
#endif
{
}

template <typename Strategy>
void VisiblePositionTemplate<Strategy>::Trace(Visitor* visitor) const {
  visitor->Trace(position_with_affinity_);
}

template <typename Strategy>
static inline bool InDifferentLinesOfSameInlineFormattingContext(
    const PositionWithAffinityTemplate<Strategy>& position1,
    const PositionWithAffinityTemplate<Strategy>& position2) {
  DCHECK(position1.IsNotNull());
  DCHECK(position2.IsNotNull());
  // Optimization for common cases.
  if (position1 == position2)
    return false;
  // InSameLine may DCHECK that the anchors have a layout object.
  if (!position1.AnchorNode()->GetLayoutObject() ||
      !position2.AnchorNode()->GetLayoutObject())
    return false;
  // Return false if the positions are in the same line.
  if (InSameLine(position1, position2))
    return false;
  // Return whether the positions are in the same inline formatting context.
  const LayoutBlockFlow* block1 =
      NGInlineFormattingContextOf(position1.GetPosition());
  return block1 &&
         block1 == NGInlineFormattingContextOf(position2.GetPosition());
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> VisiblePositionTemplate<Strategy>::Create(
    const PositionWithAffinityTemplate<Strategy>& position_with_affinity) {
  if (position_with_affinity.IsNull())
    return VisiblePositionTemplate<Strategy>();
  DCHECK(position_with_affinity.IsConnected()) << position_with_affinity;

  Document& document = *position_with_affinity.GetDocument();
  DCHECK(position_with_affinity.IsValidFor(document)) << position_with_affinity;
  DCHECK(!document.NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());

  const PositionTemplate<Strategy> deep_position =
      CanonicalPositionOf(position_with_affinity.GetPosition());
  if (deep_position.IsNull())
    return VisiblePositionTemplate<Strategy>();
  const PositionWithAffinityTemplate<Strategy> downstream_position(
      deep_position);
  if (position_with_affinity.Affinity() == TextAffinity::kDownstream) {
    // Fast path for common cases.
    if (position_with_affinity == downstream_position)
      return VisiblePositionTemplate<Strategy>(downstream_position);

    // If the canonical position went into a previous line of the same inline
    // formatting context, use the start of the current line instead.
    const PositionInFlatTree& flat_deep_position =
        ToPositionInFlatTree(deep_position);
    const PositionInFlatTree& flat_position =
        ToPositionInFlatTree(position_with_affinity.GetPosition());
    if (flat_deep_position.IsNotNull() && flat_position.IsNotNull() &&
        flat_deep_position < flat_position &&
        InDifferentLinesOfSameInlineFormattingContext(position_with_affinity,
                                                      downstream_position)) {
      const PositionWithAffinityTemplate<Strategy>& start_of_line =
          StartOfLine(position_with_affinity);
      if (start_of_line.IsNotNull())
        return VisiblePositionTemplate<Strategy>(start_of_line);
    }

    // Otherwise use the canonical position.
    return VisiblePositionTemplate<Strategy>(downstream_position);
  }

  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled() &&
      NGInlineFormattingContextOf(deep_position)) {
    // When not at a line wrap or bidi boundary, make sure to end up with
    // |TextAffinity::Downstream| affinity.
    const PositionWithAffinityTemplate<Strategy> upstream_position(
        deep_position, TextAffinity::kUpstream);

    if (AbsoluteCaretBoundsOf(downstream_position) !=
        AbsoluteCaretBoundsOf(upstream_position)) {
      return VisiblePositionTemplate<Strategy>(upstream_position);
    }
    return VisiblePositionTemplate<Strategy>(downstream_position);
  }

  // When not at a line wrap, make sure to end up with
  // |TextAffinity::Downstream| affinity.
  const PositionWithAffinityTemplate<Strategy> upstream_position(
      deep_position, TextAffinity::kUpstream);
  if (InSameLine(downstream_position, upstream_position))
    return VisiblePositionTemplate<Strategy>(downstream_position);
  return VisiblePositionTemplate<Strategy>(upstream_position);
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> VisiblePositionTemplate<Strategy>::AfterNode(
    const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::AfterNode(node)));
}

template <typename Strategy>
VisiblePositionTemplate<Strategy> VisiblePositionTemplate<Strategy>::BeforeNode(
    const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::BeforeNode(node)));
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisiblePositionTemplate<Strategy>::FirstPositionInNode(const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::FirstPositionInNode(node)));
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisiblePositionTemplate<Strategy>::InParentAfterNode(const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::InParentAfterNode(node)));
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisiblePositionTemplate<Strategy>::InParentBeforeNode(const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::InParentBeforeNode(node)));
}

template <typename Strategy>
VisiblePositionTemplate<Strategy>
VisiblePositionTemplate<Strategy>::LastPositionInNode(const Node& node) {
  return Create(PositionWithAffinityTemplate<Strategy>(
      PositionTemplate<Strategy>::LastPositionInNode(node)));
}

VisiblePosition CreateVisiblePosition(const Position& position,
                                      TextAffinity affinity) {
  return VisiblePosition::Create(PositionWithAffinity(position, affinity));
}

VisiblePosition CreateVisiblePosition(
    const PositionWithAffinity& position_with_affinity) {
  return VisiblePosition::Create(position_with_affinity);
}

VisiblePositionInFlatTree CreateVisiblePosition(
    const PositionInFlatTree& position,
    TextAffinity affinity) {
  return VisiblePositionInFlatTree::Create(
      PositionInFlatTreeWithAffinity(position, affinity));
}

VisiblePositionInFlatTree CreateVisiblePosition(
    const PositionInFlatTreeWithAffinity& position_with_affinity) {
  return VisiblePositionInFlatTree::Create(position_with_affinity);
}

#if DCHECK_IS_ON()

template <typename Strategy>
void VisiblePositionTemplate<Strategy>::ShowTreeForThis() const {
  DeepEquivalent().ShowTreeForThis();
}

#endif

template <typename Strategy>
bool VisiblePositionTemplate<Strategy>::IsValid() const {
#if DCHECK_IS_ON()
  if (IsNull())
    return true;
  Document& document = *position_with_affinity_.GetDocument();
  return dom_tree_version_ == document.DomTreeVersion() &&
         style_version_ == document.StyleVersion() &&
         !document.NeedsLayoutTreeUpdate();
#else
  return true;
#endif
}

template <typename Strategy>
bool VisiblePositionTemplate<Strategy>::IsValidFor(
    const Document& document) const {
  return position_with_affinity_.IsValidFor(document);
}

template class CORE_TEMPLATE_EXPORT VisiblePositionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    VisiblePositionTemplate<EditingInFlatTreeStrategy>;

std::ostream& operator<<(std::ostream& ostream,
                         const VisiblePosition& position) {
  return ostream << position.DeepEquivalent() << '/' << position.Affinity();
}

std::ostream& operator<<(std::ostream& ostream,
                         const VisiblePositionInFlatTree& position) {
  return ostream << position.DeepEquivalent() << '/' << position.Affinity();
}

}  // namespace blink

#if DCHECK_IS_ON()

void ShowTree(const blink::VisiblePosition* vpos) {
  if (vpos) {
    vpos->ShowTreeForThis();
    return;
  }
  DLOG(INFO) << "Cannot showTree for (nil) VisiblePosition.";
}

void ShowTree(const blink::VisiblePosition& vpos) {
  vpos.ShowTreeForThis();
}

#endif
