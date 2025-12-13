// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_registry.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_highlight_hit_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_highlights_from_point_options.h"
#include "third_party/blink/renderer/core/dom/abstract_range.h"
#include "third_party/blink/renderer/core/dom/static_range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HighlightRegistry* HighlightRegistry::From(LocalDOMWindow& window) {
  HighlightRegistry* supplement =
      Supplement<LocalDOMWindow>::From<HighlightRegistry>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<HighlightRegistry>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, supplement);
  }
  return supplement;
}

HighlightRegistry::HighlightRegistry(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window), frame_(window.GetFrame()) {}

HighlightRegistry::~HighlightRegistry() = default;

const char HighlightRegistry::kSupplementName[] = "HighlightRegistry";

void HighlightRegistry::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlights_);
  visitor->Trace(frame_);
  visitor->Trace(active_highlights_in_node_);
  ScriptWrappable::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

HighlightRegistry* HighlightRegistry::GetHighlightRegistry(const Node* node) {
  if (!node) {
    return nullptr;
  }
  return node->GetDocument()
      .domWindow()
      ->Supplementable<LocalDOMWindow>::RequireSupplement<HighlightRegistry>();
}

bool HighlightRegistry::IsAbstractRangePaintable(AbstractRange* abstract_range,
                                                 Document* document) const {
  if (abstract_range->OwnerDocument() != document ||
      abstract_range->collapsed() || !abstract_range->startContainer() ||
      !abstract_range->startContainer()->isConnected() ||
      !abstract_range->endContainer() ||
      !abstract_range->endContainer()->isConnected()) {
    return false;
  }

  auto* static_range = DynamicTo<StaticRange>(*abstract_range);
  if (static_range && !static_range->IsValid()) {
    return false;
  }

  return true;
}

// Deletes all HighlightMarkers and rebuilds them with the contents of
// highlights_.
void HighlightRegistry::ValidateHighlightMarkers() {
  Document* document = frame_->GetDocument();
  if (!document)
    return;

  // Markers are still valid if there were no changes in DOM or style and there
  // were no calls to |HighlightRegistry::ScheduleRepaint|, so we can avoid
  // rebuilding them.
  if (dom_tree_version_for_validate_highlight_markers_ ==
          document->DomTreeVersion() &&
      style_version_for_validate_highlight_markers_ ==
          document->StyleVersion() &&
      !force_markers_validation_) {
    return;
  }

  dom_tree_version_for_validate_highlight_markers_ = document->DomTreeVersion();
  style_version_for_validate_highlight_markers_ = document->StyleVersion();
  force_markers_validation_ = false;
  active_highlights_in_node_.clear();

  DocumentMarkerController& markers_controller = document->Markers();

  // We invalidate ink overflow for nodes with highlights that have visual
  // overflow, in case they no longer have markers and have smaller overflow.
  // Ideally we would only invalidate nodes with markers that
  // change their overflow status, but there is no easy way to identify those.
  // That is, the highlights associated with a node are in the document marker
  // controller, but they store the highlight name only. The actual highlight
  // style is on the node's style, but we don't know if that has changed since
  // we last computed overflow.
  HeapHashSet<WeakMember<const Text>> nodes_with_overflow;
  markers_controller.ApplyToMarkersOfType(
      [&nodes_with_overflow](const Text& node, DocumentMarker* marker) {
        auto& highlight_marker = To<CustomHighlightMarker>(*marker);
        if (highlight_marker.HasVisualOverflow()) {
          nodes_with_overflow.insert(&node);
        }
      },
      DocumentMarker::kCustomHighlight);

  // Remove all the markers, because determining which nodes have unchanged
  // marker state would be unnecessarily complex.
  markers_controller.RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::CustomHighlight());

  for (const auto& highlight_registry_map_entry : highlights_) {
    const auto& highlight_name = highlight_registry_map_entry->highlight_name;
    const auto& highlight = highlight_registry_map_entry->highlight;
    for (const auto& abstract_range : highlight->GetRanges()) {
      if (IsAbstractRangePaintable(abstract_range, document)) {
        EphemeralRange eph_range(abstract_range);
        markers_controller.AddCustomHighlightMarker(eph_range, highlight_name,
                                                    highlight);
      }
    }
  }

  // Process all of the nodes to remove overlapping custom highlights and
  // update the markers to avoid overlaps.
  markers_controller.MergeOverlappingMarkers(DocumentMarker::kCustomHighlight);

  // Set up the map of nodes to active highlights. We also need to invalidate
  // ink overflow for nodes with highlights that now have
  // visual overflow. At the same time, record the overflow status on the marker
  // so that we know that recalculation will be required when the marker is
  // removed.
  markers_controller.ApplyToMarkersOfType(
      [&nodes_with_overflow, &active = active_highlights_in_node_](
          const Text& node, DocumentMarker* marker) {
        auto& highlight_marker = To<CustomHighlightMarker>(*marker);
        const auto& iterator = active.find(&node);
        if (iterator == active.end()) {
          active.insert(&node, HashSet<AtomicString>(
                                   {highlight_marker.GetHighlightName()}));
        } else {
          iterator->value.insert(highlight_marker.GetHighlightName());
        }
        bool has_visual_overflow =
            HighlightStyleUtils::CustomHighlightHasVisualOverflow(
                node, highlight_marker.GetHighlightName());
        highlight_marker.SetHasVisualOverflow(has_visual_overflow);
        if (has_visual_overflow) {
          nodes_with_overflow.insert(&node);
        }
      },
      DocumentMarker::kCustomHighlight);

  // Invalidate all the nodes that had overflow either before or after the
  // update.
  for (auto& node : nodes_with_overflow) {
    // Explicitly cast to LayoutObject to get the correct version of
    // InvalidateVisualOverflow.
    if (LayoutObject* layout_object = node->GetLayoutObject()) {
      layout_object->InvalidateVisualOverflow();
    }
  }
}

const HashSet<AtomicString>& HighlightRegistry::GetActiveHighlights(
    const Text& node) const {
  DCHECK(active_highlights_in_node_.Contains(&node));
  return active_highlights_in_node_.find(&node)->value;
}

void HighlightRegistry::ScheduleRepaint() {
  force_markers_validation_ = true;
  if (LocalFrameView* local_frame_view = frame_->View()) {
    local_frame_view->ScheduleVisualUpdateForVisualOverflowIfNeeded();
  }
}

void HighlightRegistry::SetForTesting(AtomicString highlight_name,
                                      Highlight* highlight) {
  auto highlights_iterator = GetMapIterator(highlight_name);
  if (highlights_iterator != highlights_.end()) {
    highlights_iterator->Get()->highlight->DeregisterFrom(this);
    // It's necessary to delete it and insert a new entry to the registry
    // instead of just modifying the existing one so the insertion order is
    // preserved.
    highlights_.erase(highlights_iterator);
  }
  highlights_.insert(MakeGarbageCollected<HighlightRegistryMapEntry>(
      highlight_name, highlight, highlights_registered_++));
  highlight->RegisterIn(this);
  ScheduleRepaint();
}

void HighlightRegistry::RemoveForTesting(AtomicString highlight_name,
                                         Highlight* highlight) {
  auto highlights_iterator = GetMapIterator(highlight_name);
  if (highlights_iterator != highlights_.end()) {
    highlights_iterator->Get()->highlight->DeregisterFrom(this);
    highlights_.erase(highlights_iterator);
    ScheduleRepaint();
  }
}

HighlightRegistry* HighlightRegistry::setForBinding(
    ScriptState* script_state,
    AtomicString highlight_name,
    Member<Highlight> highlight,
    ExceptionState& exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kHighlightAPIRegisterHighlight);
  SetForTesting(highlight_name, highlight);
  return this;
}

void HighlightRegistry::clearForBinding(ScriptState*, ExceptionState&) {
  for (const auto& highlight_registry_map_entry : highlights_) {
    highlight_registry_map_entry->highlight->DeregisterFrom(this);
  }
  highlights_.clear();
  ScheduleRepaint();
}

bool HighlightRegistry::deleteForBinding(ScriptState*,
                                         const AtomicString& highlight_name,
                                         ExceptionState&) {
  auto highlights_iterator = GetMapIterator(highlight_name);
  if (highlights_iterator != highlights_.end()) {
    highlights_iterator->Get()->highlight->DeregisterFrom(this);
    highlights_.erase(highlights_iterator);
    ScheduleRepaint();
    return true;
  }

  return false;
}

int8_t HighlightRegistry::CompareOverlayStackingPosition(
    const AtomicString& highlight_name1,
    const AtomicString& highlight_name2) const {
  if (highlight_name1 == highlight_name2)
    return kOverlayStackingPositionEquivalent;

  auto highlights_iterator1 = GetMapIterator(highlight_name1);
  CHECK(highlights_iterator1 != highlights_.end());
  auto highlights_iterator2 = GetMapIterator(highlight_name2);
  CHECK(highlights_iterator2 != highlights_.end());

  if (highlights_iterator1 == highlights_.end() ||
      highlights_iterator2 == highlights_.end()) {
    return kOverlayStackingPositionEquivalent;
  }

  auto highlight_priority1 = highlights_iterator1->Get()->highlight->priority();
  auto highlight_priority2 = highlights_iterator2->Get()->highlight->priority();
  if (highlight_priority1 != highlight_priority2) {
    return highlight_priority1 > highlight_priority2
               ? kOverlayStackingPositionAbove
               : kOverlayStackingPositionBelow;
  }

  auto highlight_position1 = highlights_iterator1->Get()->registration_position;
  auto highlight_position2 = highlights_iterator2->Get()->registration_position;
  return highlight_position1 > highlight_position2
             ? kOverlayStackingPositionAbove
             : kOverlayStackingPositionBelow;
}

HighlightRegistry::IterationSource::IterationSource(
    const HighlightRegistry& highlight_registry)
    : index_(0) {
  highlights_snapshot_.ReserveInitialCapacity(
      highlight_registry.highlights_.size());
  for (const auto& highlight_registry_map_entry :
       highlight_registry.highlights_) {
    highlights_snapshot_.push_back(
        MakeGarbageCollected<HighlightRegistryMapEntry>(
            highlight_registry_map_entry));
  }
}

bool HighlightRegistry::IterationSource::FetchNextItem(ScriptState*,
                                                       String& key,
                                                       Highlight*& value) {
  if (index_ >= highlights_snapshot_.size())
    return false;
  key = highlights_snapshot_[index_]->highlight_name;
  value = highlights_snapshot_[index_++]->highlight;
  return true;
}

void HighlightRegistry::IterationSource::Trace(blink::Visitor* visitor) const {
  visitor->Trace(highlights_snapshot_);
  HighlightRegistryMapIterable::IterationSource::Trace(visitor);
}

HighlightRegistryMapIterable::IterationSource*
HighlightRegistry::CreateIterationSource(ScriptState*) {
  return MakeGarbageCollected<IterationSource>(*this);
}

HeapVector<Member<HighlightHitResult>> HighlightRegistry::highlightsFromPoint(
    float x,
    float y,
    const HighlightsFromPointOptions* options) {
  Document* document = frame_->GetDocument();
  if (!document || !document->GetLayoutView()) {
    return HeapVector<Member<HighlightHitResult>>();
  }

  Node* hit_node = HitTestInDocument(document, x, y).InnerNode();
  if (!hit_node || !hit_node->IsTextNode()) {
    return HeapVector<Member<HighlightHitResult>>();
  }

  // If the node hit is in a shadow tree whose root is not in |options|, we
  // should return no highlights.
  if (hit_node->IsInShadowTree() &&
      (!options || !options->hasShadowRoots() ||
       !options->shadowRoots().Contains(hit_node->GetTreeScope()))) {
    return HeapVector<Member<HighlightHitResult>>();
  }

  auto active_highlights_in_node_iterator =
      active_highlights_in_node_.find(To<Text>(hit_node));
  if (active_highlights_in_node_iterator == active_highlights_in_node_.end()) {
    return HeapVector<Member<HighlightHitResult>>();
  }
  Vector<AtomicString> highlight_names_at_hit_node(
      active_highlights_in_node_iterator->value);
  std::sort(highlight_names_at_hit_node.begin(),
            highlight_names_at_hit_node.end(),
            [this](const AtomicString& highlight_name1,
                   const AtomicString& highlight_name2) {
              return CompareOverlayStackingPosition(highlight_name1,
                                                    highlight_name2) ==
                     kOverlayStackingPositionAbove;
            });

  // |x| and |y| are in CSS pixels, which need to be converted to physical
  // pixels to determine if they're inside layout rectangles.
  gfx::PointF hit_point(x, y);
  hit_point.Scale(frame_->DevicePixelRatio());

  HeapVector<Member<HighlightHitResult>> highlight_hit_results;
  for (const AtomicString& highlight_name : highlight_names_at_hit_node) {
    auto highlights_iterator = GetMapIterator(highlight_name);
    CHECK(highlights_iterator != highlights_.end());
    Highlight* highlight = highlights_iterator->Get()->highlight;
    HeapVector<Member<AbstractRange>> highlight_ranges_hit;
    for (auto& abstract_range : highlight->GetRanges()) {
      // If the range starts and ends in a different tree scope than the hit
      // node (i.e., the range encloses a shadow tree), do not return it when
      // the hit is on a node inside that shadow tree. Only consider ranges
      // within the same tree scope as the hit node.
      if (abstract_range->startContainer()->GetTreeScope() !=
          hit_node->GetTreeScope()) {
        continue;
      }

      if (IsAbstractRangePaintable(abstract_range, document)) {
        EphemeralRange ephemeral_range(abstract_range);
        Vector<gfx::QuadF> quads = ComputeTextBounds(ephemeral_range);
        for (const auto& quad : quads) {
          if (quad.Contains(hit_point)) {
            highlight_ranges_hit.push_back(abstract_range);
            break;
          }
        }
      }
    }

    if (highlight_ranges_hit.size()) {
      HighlightHitResult* highlight_hit_result =
          MakeGarbageCollected<HighlightHitResult>();
      highlight_hit_result->setHighlight(highlight);
      highlight_hit_result->setRanges(highlight_ranges_hit);
      highlight_hit_results.push_back(highlight_hit_result);
    }
  }

  return highlight_hit_results;
}
}  // namespace blink
