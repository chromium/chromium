/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"

#include <algorithm>
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/active_suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/active_suggestion_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/grammar_marker.h"
#include "third_party/blink/renderer/core/editing/markers/grammar_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/sorted_document_marker_list_editor.h"
#include "third_party/blink/renderer/core/editing/markers/spelling_marker.h"
#include "third_party/blink/renderer/core/editing/markers/spelling_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker_list_impl.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

namespace {

DocumentMarker::MarkerTypeIndex MarkerTypeToMarkerIndex(
    DocumentMarker::MarkerType type) {
  switch (type) {
    case DocumentMarker::kSpelling:
      return DocumentMarker::kSpellingMarkerIndex;
    case DocumentMarker::kGrammar:
      return DocumentMarker::kGrammarMarkerIndex;
    case DocumentMarker::kTextMatch:
      return DocumentMarker::kTextMatchMarkerIndex;
    case DocumentMarker::kComposition:
      return DocumentMarker::kCompositionMarkerIndex;
    case DocumentMarker::kActiveSuggestion:
      return DocumentMarker::kActiveSuggestionMarkerIndex;
    case DocumentMarker::kSuggestion:
      return DocumentMarker::kSuggestionMarkerIndex;
    case DocumentMarker::kTextFragment:
      return DocumentMarker::kTextFragmentMarkerIndex;
  }

  NOTREACHED();
  return DocumentMarker::kSpellingMarkerIndex;
}

DocumentMarkerList* CreateListForType(DocumentMarker::MarkerType type) {
  switch (type) {
    case DocumentMarker::kActiveSuggestion:
      return MakeGarbageCollected<ActiveSuggestionMarkerListImpl>();
    case DocumentMarker::kComposition:
      return MakeGarbageCollected<CompositionMarkerListImpl>();
    case DocumentMarker::kSpelling:
      return MakeGarbageCollected<SpellingMarkerListImpl>();
    case DocumentMarker::kGrammar:
      return MakeGarbageCollected<GrammarMarkerListImpl>();
    case DocumentMarker::kSuggestion:
      return MakeGarbageCollected<SuggestionMarkerListImpl>();
    case DocumentMarker::kTextMatch:
      return MakeGarbageCollected<TextMatchMarkerListImpl>();
    case DocumentMarker::kTextFragment:
      return MakeGarbageCollected<TextFragmentMarkerListImpl>();
  }

  NOTREACHED();
  return nullptr;
}

void InvalidatePaintForNode(const Node& node) {
  if (!node.GetLayoutObject())
    return;

  node.GetLayoutObject()->SetShouldDoFullPaintInvalidation(
      PaintInvalidationReason::kDocumentMarker);

  // Tell accessibility about the new marker.
  AXObjectCache* ax_object_cache = node.GetDocument().ExistingAXObjectCache();
  if (!ax_object_cache)
    return;
  // TODO(nektar): Do major refactoring of all AX classes to comply with const
  // correctness.
  Node* non_const_node = &const_cast<Node&>(node);
  ax_object_cache->HandleTextMarkerDataAdded(non_const_node, non_const_node);
}

}  // namespace

Member<DocumentMarkerList>& DocumentMarkerController::ListForType(
    MarkerLists* marker_lists,
    DocumentMarker::MarkerType type) {
  const wtf_size_t marker_list_index = MarkerTypeToMarkerIndex(type);
  return (*marker_lists)[marker_list_index];
}

bool DocumentMarkerController::PossiblyHasMarkers(
    DocumentMarker::MarkerType type) const {
  return PossiblyHasMarkers(DocumentMarker::MarkerTypes(type));
}

inline bool DocumentMarkerController::PossiblyHasMarkers(
    DocumentMarker::MarkerTypes types) const {
  DCHECK(!markers_.IsEmpty() ||
         possibly_existing_marker_types_ == DocumentMarker::MarkerTypes(0));
  return possibly_existing_marker_types_.Intersects(types);
}

DocumentMarkerController::DocumentMarkerController(Document& document)
    : document_(&document) {}

void DocumentMarkerController::Clear() {
  markers_.clear();
  possibly_existing_marker_types_ = DocumentMarker::MarkerTypes();
  SetContext(nullptr);
}

void DocumentMarkerController::AddSpellingMarker(const EphemeralRange& range,
                                                 const String& description) {
  AddMarkerInternal(range, [&description](int start_offset, int end_offset) {
    return MakeGarbageCollected<SpellingMarker>(start_offset, end_offset,
                                                description);
  });
}

void DocumentMarkerController::AddGrammarMarker(const EphemeralRange& range,
                                                const String& description) {
  AddMarkerInternal(range, [&description](int start_offset, int end_offset) {
    return MakeGarbageCollected<GrammarMarker>(start_offset, end_offset,
                                               description);
  });
}

void DocumentMarkerController::AddTextMatchMarker(
    const EphemeralRange& range,
    TextMatchMarker::MatchStatus match_status) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(range, [match_status](int start_offset, int end_offset) {
    return MakeGarbageCollected<TextMatchMarker>(start_offset, end_offset,
                                                 match_status);
  });
  // Don't invalidate tickmarks here. TextFinder invalidates tickmarks using a
  // throttling algorithm. crbug.com/6819.
}

void DocumentMarkerController::AddCompositionMarker(
    const EphemeralRange& range,
    Color underline_color,
    ui::mojom::ImeTextSpanThickness thickness,
    Color background_color) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(range, [underline_color, thickness, background_color](
                               int start_offset, int end_offset) {
    return MakeGarbageCollected<CompositionMarker>(
        start_offset, end_offset, underline_color, thickness, background_color);
  });
}

void DocumentMarkerController::AddActiveSuggestionMarker(
    const EphemeralRange& range,
    Color underline_color,
    ui::mojom::ImeTextSpanThickness thickness,
    Color background_color) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(range, [underline_color, thickness, background_color](
                               int start_offset, int end_offset) {
    return MakeGarbageCollected<ActiveSuggestionMarker>(
        start_offset, end_offset, underline_color, thickness, background_color);
  });
}

void DocumentMarkerController::AddSuggestionMarker(
    const EphemeralRange& range,
    const SuggestionMarkerProperties& properties) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(range, [&properties](int start_offset, int end_offset) {
    return MakeGarbageCollected<SuggestionMarker>(start_offset, end_offset,
                                                  properties);
  });
}

void DocumentMarkerController::AddTextFragmentMarker(
    const EphemeralRange& range) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(range, [](int start_offset, int end_offset) {
    return MakeGarbageCollected<TextFragmentMarker>(start_offset, end_offset);
  });
}

void DocumentMarkerController::PrepareForDestruction() {
  Clear();
}

void DocumentMarkerController::RemoveMarkers(
    TextIterator& marked_text,
    DocumentMarker::MarkerTypes marker_types) {
  for (; !marked_text.AtEnd(); marked_text.Advance()) {
    if (!PossiblyHasMarkers(marker_types))
      return;
    DCHECK(!markers_.IsEmpty());
    const Node& node = marked_text.CurrentContainer();
    auto* text_node = DynamicTo<Text>(node);
    if (!text_node)
      continue;
    int start_offset = marked_text.StartOffsetInCurrentContainer();
    int end_offset = marked_text.EndOffsetInCurrentContainer();
    RemoveMarkersInternal(*text_node, start_offset, end_offset - start_offset,
                          marker_types);
  }
}

void DocumentMarkerController::RemoveMarkersInRange(
    const EphemeralRange& range,
    DocumentMarker::MarkerTypes marker_types) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());

  TextIterator marked_text(range.StartPosition(), range.EndPosition());
  DocumentMarkerController::RemoveMarkers(marked_text, marker_types);
}

void DocumentMarkerController::AddMarkerInternal(
    const EphemeralRange& range,
    std::function<DocumentMarker*(int, int)> create_marker_from_offsets) {
  for (TextIterator marked_text(range.StartPosition(), range.EndPosition());
       !marked_text.AtEnd(); marked_text.Advance()) {
    const int start_offset_in_current_container =
        marked_text.StartOffsetInCurrentContainer();
    const int end_offset_in_current_container =
        marked_text.EndOffsetInCurrentContainer();

    DCHECK_GE(end_offset_in_current_container,
              start_offset_in_current_container);

    // TODO(editing-dev): TextIterator sometimes emits ranges where the start
    // and end offsets are the same. Investigate if TextIterator should be
    // changed to not do this. See crbug.com/727929
    if (end_offset_in_current_container == start_offset_in_current_container)
      continue;

    // Ignore text emitted by TextIterator for non-text nodes (e.g. implicit
    // newlines)
    const auto* text_node = DynamicTo<Text>(marked_text.CurrentContainer());
    if (!text_node)
      continue;

    DocumentMarker* const new_marker = create_marker_from_offsets(
        start_offset_in_current_container, end_offset_in_current_container);
    AddMarkerToNode(*text_node, new_marker);
  }
}

void DocumentMarkerController::AddMarkerToNode(const Text& text,
                                               DocumentMarker* new_marker) {
  DCHECK_GE(text.length(), new_marker->EndOffset());
  possibly_existing_marker_types_ = possibly_existing_marker_types_.Add(
      DocumentMarker::MarkerTypes(new_marker->GetType()));
  SetContext(document_);

  Member<MarkerLists>& markers =
      markers_.insert(&text, nullptr).stored_value->value;
  if (!markers) {
    markers = MakeGarbageCollected<MarkerLists>();
    markers->Grow(DocumentMarker::kMarkerTypeIndexesCount);
  }

  const DocumentMarker::MarkerType new_marker_type = new_marker->GetType();
  if (!ListForType(markers, new_marker_type))
    ListForType(markers, new_marker_type) = CreateListForType(new_marker_type);

  DocumentMarkerList* const list = ListForType(markers, new_marker_type);
  list->Add(new_marker);

  InvalidatePaintForNode(text);
}

// Moves markers from src_node to dst_node. Markers are moved if their start
// offset is less than length. Markers that run past that point are truncated.
void DocumentMarkerController::MoveMarkers(const Text& src_node,
                                           int length,
                                           const Text& dst_node) {
  if (length <= 0)
    return;

  if (!PossiblyHasMarkers(DocumentMarker::MarkerTypes::All()))
    return;
  DCHECK(!markers_.IsEmpty());

  MarkerLists* const src_markers = markers_.at(&src_node);
  if (!src_markers)
    return;

  if (!markers_.Contains(&dst_node)) {
    markers_.insert(&dst_node, MakeGarbageCollected<MarkerLists>(
                                   DocumentMarker::kMarkerTypeIndexesCount));
  }
  MarkerLists* const dst_markers = markers_.at(&dst_node);

  bool doc_dirty = false;
  for (DocumentMarker::MarkerType type : DocumentMarker::MarkerTypes::All()) {
    DocumentMarkerList* const src_list = ListForType(src_markers, type);
    if (!src_list)
      continue;

    if (!ListForType(dst_markers, type))
      ListForType(dst_markers, type) = CreateListForType(type);

    DocumentMarkerList* const dst_list = ListForType(dst_markers, type);
    if (src_list->MoveMarkers(length, dst_list))
      doc_dirty = true;
  }

  if (!doc_dirty)
    return;

  InvalidatePaintForNode(dst_node);
}

void DocumentMarkerController::RemoveMarkersInternal(
    const Text& text,
    unsigned start_offset,
    int length,
    DocumentMarker::MarkerTypes marker_types) {
  if (length <= 0)
    return;

  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!(markers_.IsEmpty()));

  MarkerLists* const markers = markers_.at(&text);
  if (!markers)
    return;

  bool doc_dirty = false;
  size_t empty_lists_count = 0;
  for (DocumentMarker::MarkerType type : DocumentMarker::MarkerTypes::All()) {
    DocumentMarkerList* const list = ListForType(markers, type);
    if (!list || list->IsEmpty()) {
      if (list && list->IsEmpty())
        ListForType(markers, type) = nullptr;
      ++empty_lists_count;
      continue;
    }
    if (!marker_types.Contains(type))
      continue;

    if (list->RemoveMarkers(start_offset, length))
      doc_dirty = true;

    if (list->IsEmpty()) {
      ListForType(markers, type) = nullptr;
      ++empty_lists_count;
    }
  }

  if (empty_lists_count == DocumentMarker::kMarkerTypeIndexesCount) {
    markers_.erase(&text);
    if (markers_.IsEmpty()) {
      possibly_existing_marker_types_ = DocumentMarker::MarkerTypes();
      SetContext(nullptr);
    }
  }

  if (!doc_dirty)
    return;

  InvalidatePaintForNode(text);
}

DocumentMarker* DocumentMarkerController::FirstMarkerAroundPosition(
    const PositionInFlatTree& position,
    DocumentMarker::MarkerTypes types) {
  if (position.IsNull())
    return nullptr;

  const PositionInFlatTree start_of_word_or_null =
      StartOfWordPosition(position, kPreviousWordIfOnBoundary);
  const PositionInFlatTree start =
      start_of_word_or_null.IsNotNull() ? start_of_word_or_null : position;
  const PositionInFlatTree end_of_word_or_null =
      EndOfWordPosition(position, kNextWordIfOnBoundary);
  const PositionInFlatTree end =
      end_of_word_or_null.IsNotNull() ? end_of_word_or_null : position;

  if (start > end) {
    // TODO(crbug.com/778507): We shouldn't reach here, but currently do due to
    // legacy implementation of StartOfWord(). Rewriting StartOfWord() with
    // TextOffsetMapping should fix it.
    NOTREACHED() << "|start| should be before |end|.";
    return nullptr;
  }

  const Node* const start_node = start.ComputeContainerNode();
  const unsigned start_offset = start.ComputeOffsetInContainerNode();
  const Node* const end_node = end.ComputeContainerNode();
  const unsigned end_offset = end.ComputeOffsetInContainerNode();

  for (const Node& node : EphemeralRangeInFlatTree(start, end).Nodes()) {
    auto* text_node = DynamicTo<Text>(node);
    if (!text_node)
      continue;

    const unsigned start_range_offset = node == start_node ? start_offset : 0;
    const unsigned end_range_offset =
        node == end_node ? end_offset : text_node->length();

    DocumentMarker* const found_marker = FirstMarkerIntersectingOffsetRange(
        *text_node, start_range_offset, end_range_offset, types);
    if (found_marker)
      return found_marker;
  }

  return nullptr;
}

DocumentMarker* DocumentMarkerController::FirstMarkerIntersectingEphemeralRange(
    const EphemeralRange& range,
    DocumentMarker::MarkerTypes types) {
  if (range.IsNull())
    return nullptr;

  if (range.IsCollapsed()) {
    return FirstMarkerAroundPosition(
        ToPositionInFlatTree(range.StartPosition()), types);
  }

  const Node* const start_container =
      range.StartPosition().ComputeContainerNode();
  const Node* const end_container = range.EndPosition().ComputeContainerNode();

  // We don't currently support the case where a marker spans multiple nodes.
  // See crbug.com/720065
  if (start_container != end_container)
    return nullptr;

  auto* text_node = DynamicTo<Text>(start_container);
  if (!text_node)
    return nullptr;

  const unsigned start_offset =
      range.StartPosition().ComputeOffsetInContainerNode();
  const unsigned end_offset =
      range.EndPosition().ComputeOffsetInContainerNode();

  return FirstMarkerIntersectingOffsetRange(*text_node, start_offset,
                                            end_offset, types);
}

DocumentMarker* DocumentMarkerController::FirstMarkerIntersectingOffsetRange(
    const Text& node,
    unsigned start_offset,
    unsigned end_offset,
    DocumentMarker::MarkerTypes types) {
  if (!PossiblyHasMarkers(types))
    return nullptr;

  // Minor optimization: if we have an empty range at a node boundary, it
  // doesn't fall in the interior of any marker.
  if (start_offset == 0 && end_offset == 0)
    return nullptr;
  const unsigned node_length = node.length();
  if (start_offset == node_length && end_offset == node_length)
    return nullptr;

  MarkerLists* const markers = markers_.at(&node);
  if (!markers)
    return nullptr;

  for (DocumentMarker::MarkerType type : types) {
    const DocumentMarkerList* const list = ListForType(markers, type);
    if (!list)
      continue;

    DocumentMarker* found_marker =
        list->FirstMarkerIntersectingRange(start_offset, end_offset);
    if (found_marker)
      return found_marker;
  }

  return nullptr;
}

HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>
DocumentMarkerController::MarkersIntersectingRange(
    const EphemeralRangeInFlatTree& range,
    DocumentMarker::MarkerTypes types) {
  HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>
      node_marker_pairs;
  if (!PossiblyHasMarkers(types))
    return node_marker_pairs;

  const Node* const range_start_container =
      range.StartPosition().ComputeContainerNode();
  const unsigned range_start_offset =
      range.StartPosition().ComputeOffsetInContainerNode();
  const Node* const range_end_container =
      range.EndPosition().ComputeContainerNode();
  const unsigned range_end_offset =
      range.EndPosition().ComputeOffsetInContainerNode();

  for (Node& node : range.Nodes()) {
    auto* text_node = DynamicTo<Text>(node);
    if (!text_node)
      continue;
    MarkerLists* const markers = markers_.at(text_node);
    if (!markers)
      continue;

    for (DocumentMarker::MarkerType type : types) {
      const DocumentMarkerList* const list = ListForType(markers, type);
      if (!list)
        continue;

      const unsigned start_offset =
          node == range_start_container ? range_start_offset : 0;
      const unsigned max_character_offset = To<CharacterData>(node).length();
      const unsigned end_offset =
          node == range_end_container ? range_end_offset : max_character_offset;

      // Minor optimization: if we have an empty offset range at the boundary
      // of a text node, it doesn't fall into the interior of any marker.
      if (start_offset == 0 && end_offset == 0)
        continue;
      if (start_offset == max_character_offset && end_offset == 0)
        continue;

      const DocumentMarkerVector& markers_from_this_list =
          list->MarkersIntersectingRange(start_offset, end_offset);
      for (DocumentMarker* marker : markers_from_this_list)
        node_marker_pairs.push_back(std::make_pair(&To<Text>(node), marker));
    }
  }

  return node_marker_pairs;
}

DocumentMarkerVector DocumentMarkerController::MarkersFor(
    const Text& text,
    DocumentMarker::MarkerTypes marker_types) const {
  DocumentMarkerVector result;
  if (!PossiblyHasMarkers(marker_types))
    return result;

  MarkerLists* markers = markers_.at(&text);
  if (!markers)
    return result;

  for (DocumentMarker::MarkerType type : marker_types) {
    DocumentMarkerList* const list = ListForType(markers, type);
    if (!list || list->IsEmpty())
      continue;

    result.AppendVector(list->GetMarkers());
  }

  std::sort(result.begin(), result.end(),
            [](const Member<DocumentMarker>& marker1,
               const Member<DocumentMarker>& marker2) {
              return marker1->StartOffset() < marker2->StartOffset();
            });
  return result;
}

DocumentMarkerVector DocumentMarkerController::Markers() const {
  DocumentMarkerVector result;
  for (const auto& node_markers : markers_) {
    MarkerLists* markers = node_markers.value;
    for (DocumentMarker::MarkerType type : DocumentMarker::MarkerTypes::All()) {
      DocumentMarkerList* const list = ListForType(markers, type);
      if (!list)
        continue;
      result.AppendVector(list->GetMarkers());
    }
  }
  std::sort(result.begin(), result.end(),
            [](const Member<DocumentMarker>& marker1,
               const Member<DocumentMarker>& marker2) {
              return marker1->StartOffset() < marker2->StartOffset();
            });
  return result;
}

DocumentMarkerVector DocumentMarkerController::ComputeMarkersToPaint(
    const Text& text) const {
  // We don't render composition or spelling markers that overlap suggestion
  // markers.
  // Note: DocumentMarkerController::MarkersFor() returns markers sorted by
  // start offset.
  const DocumentMarkerVector& suggestion_markers =
      MarkersFor(text, DocumentMarker::MarkerTypes::Suggestion());
  if (suggestion_markers.IsEmpty()) {
    // If there are no suggestion markers, we can return early as a minor
    // performance optimization.
    return MarkersFor(
        text, DocumentMarker::MarkerTypes::AllBut(
                  DocumentMarker::MarkerTypes(DocumentMarker::kSuggestion)));
  }

  const DocumentMarkerVector& markers_overridden_by_suggestion_markers =
      MarkersFor(text,
                 DocumentMarker::MarkerTypes(DocumentMarker::kComposition |
                                             DocumentMarker::kSpelling));

  Vector<unsigned> suggestion_starts;
  Vector<unsigned> suggestion_ends;
  for (const DocumentMarker* suggestion_marker : suggestion_markers) {
    suggestion_starts.push_back(suggestion_marker->StartOffset());
    suggestion_ends.push_back(suggestion_marker->EndOffset());
  }

  std::sort(suggestion_starts.begin(), suggestion_starts.end());
  std::sort(suggestion_ends.begin(), suggestion_ends.end());

  unsigned suggestion_starts_index = 0;
  unsigned suggestion_ends_index = 0;
  unsigned number_suggestions_currently_inside = 0;

  DocumentMarkerVector markers_to_paint;
  for (DocumentMarker* marker : markers_overridden_by_suggestion_markers) {
    while (suggestion_starts_index < suggestion_starts.size() &&
           suggestion_starts[suggestion_starts_index] <=
               marker->StartOffset()) {
      ++suggestion_starts_index;
      ++number_suggestions_currently_inside;
    }
    while (suggestion_ends_index < suggestion_ends.size() &&
           suggestion_ends[suggestion_ends_index] <= marker->StartOffset()) {
      ++suggestion_ends_index;
      --number_suggestions_currently_inside;
    }

    // At this point, number_suggestions_currently_inside should be equal to the
    // number of suggestion markers overlapping the point marker->StartOffset()
    // (marker endpoints don't count as overlapping).

    // Marker is overlapped by a suggestion marker, do not paint.
    if (number_suggestions_currently_inside)
      continue;

    // Verify that no suggestion marker starts before the current marker ends.
    if (suggestion_starts_index < suggestion_starts.size() &&
        suggestion_starts[suggestion_starts_index] < marker->EndOffset())
      continue;

    markers_to_paint.push_back(marker);
  }

  markers_to_paint.AppendVector(suggestion_markers);

  markers_to_paint.AppendVector(MarkersFor(
      text, DocumentMarker::MarkerTypes::AllBut(DocumentMarker::MarkerTypes(
                DocumentMarker::kComposition | DocumentMarker::kSpelling |
                DocumentMarker::kSuggestion))));

  return markers_to_paint;
}

bool DocumentMarkerController::PossiblyHasTextMatchMarkers() const {
  return PossiblyHasMarkers(DocumentMarker::kTextMatch);
}

Vector<IntRect> DocumentMarkerController::LayoutRectsForTextMatchMarkers() {
  DCHECK(!document_->View()->NeedsLayout());
  DCHECK(!document_->NeedsLayoutTreeUpdate());

  Vector<IntRect> result;

  if (!PossiblyHasMarkers(DocumentMarker::kTextMatch))
    return result;
  DCHECK(!(markers_.IsEmpty()));

  // outer loop: process each node
  MarkerMap::iterator end = markers_.end();
  for (MarkerMap::iterator node_iterator = markers_.begin();
       node_iterator != end; ++node_iterator) {
    // inner loop; process each marker in this node
    const Node& node = *node_iterator->key;
    if (!node.isConnected())
      continue;
    MarkerLists* markers = node_iterator->value.Get();
    DocumentMarkerList* const list =
        ListForType(markers, DocumentMarker::kTextMatch);
    if (!list)
      continue;
    result.AppendVector(To<TextMatchMarkerListImpl>(list)->LayoutRects(node));
  }

  return result;
}

static void InvalidatePaintForTickmarks(const Node& node) {
  if (LayoutView* layout_view = node.GetDocument().GetLayoutView())
    layout_view->InvalidatePaintForTickmarks();
}

void DocumentMarkerController::InvalidateRectsForTextMatchMarkersInNode(
    const Text& node) {
  MarkerLists* markers = markers_.at(&node);

  const DocumentMarkerList* const marker_list =
      ListForType(markers, DocumentMarker::kTextMatch);
  if (!marker_list || marker_list->IsEmpty())
    return;

  const HeapVector<Member<DocumentMarker>>& markers_in_list =
      marker_list->GetMarkers();
  for (auto& marker : markers_in_list)
    To<TextMatchMarker>(marker.Get())->Invalidate();

  InvalidatePaintForTickmarks(node);
}

void DocumentMarkerController::InvalidateRectsForAllTextMatchMarkers() {
  for (auto& node_markers : markers_) {
    const Text& node = *node_markers.key;
    InvalidateRectsForTextMatchMarkersInNode(node);
  }
}

void DocumentMarkerController::DidProcessMarkerMap(const WeakCallbackInfo&) {
  if (markers_.IsEmpty())
    Clear();
}

void DocumentMarkerController::Trace(Visitor* visitor) {
  // Note: To make |DidProcessMarkerMap()| called after weak members callback
  // of |markers_|, we should register it before tracing |markers_|.
  visitor->template RegisterWeakCallbackMethod<
      DocumentMarkerController, &DocumentMarkerController::DidProcessMarkerMap>(
      this);
  visitor->Trace(markers_);
  visitor->Trace(document_);
  SynchronousMutationObserver::Trace(visitor);
}

void DocumentMarkerController::RemoveMarkersForNode(
    const Text& text,
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!markers_.IsEmpty());

  MarkerMap::iterator iterator = markers_.find(&text);
  if (iterator != markers_.end())
    RemoveMarkersFromList(iterator, marker_types);
}

void DocumentMarkerController::RemoveSpellingMarkersUnderWords(
    const Vector<String>& words) {
  for (auto& node_markers : markers_) {
    const Text& text = *node_markers.key;
    MarkerLists* markers = node_markers.value;
    for (DocumentMarker::MarkerType type :
         DocumentMarker::MarkerTypes::Misspelling()) {
      DocumentMarkerList* const list = ListForType(markers, type);
      if (!list)
        continue;
      if (To<SpellCheckMarkerListImpl>(list)->RemoveMarkersUnderWords(
              text.data(), words)) {
        InvalidatePaintForNode(text);
      }
    }
  }
}

void DocumentMarkerController::RemoveSuggestionMarkerInRangeOnFinish(
    const EphemeralRangeInFlatTree& range) {
  // MarkersIntersectingRange() might be expensive. In practice, we hope we will
  // only check one node for composing range.
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      node_marker_pairs = MarkersIntersectingRange(
          range, DocumentMarker::MarkerTypes::Suggestion());
  for (const auto& node_marker_pair : node_marker_pairs) {
    auto* suggestion_marker =
        To<SuggestionMarker>(node_marker_pair.second.Get());
    if (suggestion_marker->NeedsRemovalOnFinishComposing()) {
      const Text& text = *node_marker_pair.first;
      DocumentMarkerList* const list =
          ListForType(markers_.at(&text), DocumentMarker::kSuggestion);
      // RemoveMarkerByTag() might be expensive. In practice, we have at most
      // one suggestion marker needs to be removed.
      To<SuggestionMarkerListImpl>(list)->RemoveMarkerByTag(
          suggestion_marker->Tag());
      InvalidatePaintForNode(text);
    }
  }
}

void DocumentMarkerController::RemoveSuggestionMarkerByTag(const Text& text,
                                                           int32_t marker_tag) {
  MarkerLists* markers = markers_.at(&text);
  auto* const list = To<SuggestionMarkerListImpl>(
      ListForType(markers, DocumentMarker::kSuggestion).Get());
  if (!list->RemoveMarkerByTag(marker_tag))
    return;
  InvalidatePaintForNode(text);
}

void DocumentMarkerController::RemoveMarkersOfTypes(
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!markers_.IsEmpty());

  HeapVector<Member<const Text>> nodes_with_markers;
  CopyKeysToVector(markers_, nodes_with_markers);
  unsigned size = nodes_with_markers.size();
  for (unsigned i = 0; i < size; ++i) {
    MarkerMap::iterator iterator = markers_.find(nodes_with_markers[i]);
    if (iterator != markers_.end())
      RemoveMarkersFromList(iterator, marker_types);
  }

  if (PossiblyHasMarkers(DocumentMarker::MarkerTypes::AllBut(marker_types)))
    return;
  SetContext(nullptr);
}

void DocumentMarkerController::RemoveMarkersFromList(
    MarkerMap::iterator iterator,
    DocumentMarker::MarkerTypes marker_types) {
  bool needs_repainting = false;
  bool node_can_be_removed;

  size_t empty_lists_count = 0;
  if (marker_types == DocumentMarker::MarkerTypes::All()) {
    needs_repainting = true;
    node_can_be_removed = true;
  } else {
    MarkerLists* markers = iterator->value.Get();

    for (DocumentMarker::MarkerType type : DocumentMarker::MarkerTypes::All()) {
      DocumentMarkerList* const list = ListForType(markers, type);
      if (!list || list->IsEmpty()) {
        if (list && list->IsEmpty())
          ListForType(markers, type) = nullptr;
        ++empty_lists_count;
        continue;
      }
      if (marker_types.Contains(type)) {
        list->Clear();
        ListForType(markers, type) = nullptr;
        ++empty_lists_count;
        needs_repainting = true;
      }
    }

    node_can_be_removed =
        empty_lists_count == DocumentMarker::kMarkerTypeIndexesCount;
  }

  if (needs_repainting) {
    const Text& node = *iterator->key;
    InvalidatePaintForNode(node);
    InvalidatePaintForTickmarks(node);
  }

  if (node_can_be_removed) {
    markers_.erase(iterator);
    if (markers_.IsEmpty()) {
      possibly_existing_marker_types_ = DocumentMarker::MarkerTypes();
      SetContext(nullptr);
    }
  }
}

void DocumentMarkerController::RepaintMarkers(
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!markers_.IsEmpty());

  // outer loop: process each markered Text in the document
  for (auto& iterator : markers_) {
    // inner loop: process each marker in the current Text
    MarkerLists* markers = iterator.value.Get();
    for (DocumentMarker::MarkerType type : DocumentMarker::MarkerTypes::All()) {
      DocumentMarkerList* const list = ListForType(markers, type);
      if (!list || list->IsEmpty() || !marker_types.Contains(type))
        continue;

      InvalidatePaintForNode(*iterator.key);
    }
  }
}

bool DocumentMarkerController::SetTextMatchMarkersActive(
    const EphemeralRange& range,
    bool active) {
  if (!PossiblyHasMarkers(DocumentMarker::kTextMatch))
    return false;

  DCHECK(!markers_.IsEmpty());

  const Node* const start_container =
      range.StartPosition().ComputeContainerNode();
  DCHECK(start_container);
  const Node* const end_container = range.EndPosition().ComputeContainerNode();
  DCHECK(end_container);

  const unsigned container_start_offset =
      range.StartPosition().ComputeOffsetInContainerNode();
  const unsigned container_end_offset =
      range.EndPosition().ComputeOffsetInContainerNode();

  bool marker_found = false;
  for (Node& node : range.Nodes()) {
    auto* text_node = DynamicTo<Text>(node);
    if (!text_node)
      continue;
    int start_offset = node == start_container ? container_start_offset : 0;
    int end_offset = node == end_container ? container_end_offset : INT_MAX;
    marker_found |=
        SetTextMatchMarkersActive(*text_node, start_offset, end_offset, active);
  }
  return marker_found;
}

bool DocumentMarkerController::SetTextMatchMarkersActive(const Text& text,
                                                         unsigned start_offset,
                                                         unsigned end_offset,
                                                         bool active) {
  MarkerLists* markers = markers_.at(&text);
  if (!markers)
    return false;

  DocumentMarkerList* const list =
      ListForType(markers, DocumentMarker::kTextMatch);
  if (!list)
    return false;

  bool doc_dirty = To<TextMatchMarkerListImpl>(list)->SetTextMatchMarkersActive(
      start_offset, end_offset, active);

  if (!doc_dirty)
    return false;
  InvalidatePaintForNode(text);
  return true;
}

#if DCHECK_IS_ON()
void DocumentMarkerController::ShowMarkers() const {
  StringBuilder builder;
  for (auto& node_iterator : markers_) {
    const Text* node = node_iterator.key;
    builder.AppendFormat("%p", node);
    MarkerLists* markers = markers_.at(node);
    for (DocumentMarker::MarkerType type : DocumentMarker::MarkerTypes::All()) {
      DocumentMarkerList* const list = ListForType(markers, type);
      if (!list)
        continue;

      const HeapVector<Member<DocumentMarker>>& markers_in_list =
          list->GetMarkers();
      for (const DocumentMarker* marker : markers_in_list) {
        bool is_active_match = false;
        if (auto* text_match = DynamicTo<TextMatchMarker>(marker))
          is_active_match = text_match->IsActiveMatch();

        builder.AppendFormat(
            " %u:[%u:%u](%d)", static_cast<uint32_t>(marker->GetType()),
            marker->StartOffset(), marker->EndOffset(), is_active_match);
      }
    }
    builder.Append("\n");
  }
  LOG(INFO) << markers_.size() << " nodes have markers:\n"
            << builder.ToString().Utf8();
}
#endif

// SynchronousMutationObserver
void DocumentMarkerController::DidUpdateCharacterData(CharacterData* node,
                                                      unsigned offset,
                                                      unsigned old_length,
                                                      unsigned new_length) {
  if (!PossiblyHasMarkers(DocumentMarker::MarkerTypes::All()))
    return;
  DCHECK(!markers_.IsEmpty());
  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return;
  MarkerLists* markers = markers_.at(text_node);
  if (!markers)
    return;

  bool did_shift_marker = false;
  for (DocumentMarkerList* const list : *markers) {
    if (!list)
      continue;

    if (list->ShiftMarkers(node->data(), offset, old_length, new_length))
      did_shift_marker = true;
  }

  if (!did_shift_marker)
    return;
  if (!node->GetLayoutObject())
    return;
  InvalidateRectsForTextMatchMarkersInNode(*text_node);
  InvalidatePaintForNode(*node);
}

}  // namespace blink

#if DCHECK_IS_ON()
void showDocumentMarkers(const blink::DocumentMarkerController* controller) {
  if (controller)
    controller->ShowMarkers();
}
#endif
