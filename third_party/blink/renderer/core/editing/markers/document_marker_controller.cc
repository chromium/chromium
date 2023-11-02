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
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker_list_impl.h"
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
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
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
    case DocumentMarker::kCustomHighlight:
      return DocumentMarker::kCustomHighlightMarkerIndex;
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
    case DocumentMarker::kCustomHighlight:
      return MakeGarbageCollected<CustomHighlightMarkerListImpl>();
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

PositionInFlatTree SearchAroundPositionStart(
    const PositionInFlatTree& position) {
  const PositionInFlatTree start_of_word_or_null =
      StartOfWordPosition(position, kPreviousWordIfOnBoundary);
  return start_of_word_or_null.IsNotNull() ? start_of_word_or_null : position;
}

PositionInFlatTree SearchAroundPositionEnd(const PositionInFlatTree& position) {
  const PositionInFlatTree end_of_word_or_null =
      EndOfWordPosition(position, kNextWordIfOnBoundary);
  return end_of_word_or_null.IsNotNull() ? end_of_word_or_null : position;
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
  DCHECK(!markers_.empty() ||
         possibly_existing_marker_types_ == DocumentMarker::MarkerTypes(0));
  return possibly_existing_marker_types_.Intersects(types);
}

DocumentMarkerController::DocumentMarkerController(Document& document)
    : document_(&document) {}

void DocumentMarkerController::Clear() {
  markers_.clear();
  possibly_existing_marker_types_ = DocumentMarker::MarkerTypes();
  SetDocument(nullptr);
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
  AddMarkerInternal(
      range,
      [match_status](int start_offset, int end_offset) {
        return MakeGarbageCollected<TextMatchMarker>(start_offset, end_offset,
                                                     match_status);
      },
      // Since we've already determined to have a match in the given range (via
      // FindBuffer), we can ignore the display lock for the purposes of finding
      // where to put the marker.
      TextIteratorBehavior::Builder().SetIgnoresDisplayLock(true).Build());
  // Don't invalidate tickmarks here. TextFinder invalidates tickmarks using a
  // throttling algorithm. crbug.com/6819.
}

void DocumentMarkerController::AddCompositionMarker(
    const EphemeralRange& range,
    Color underline_color,
    ui::mojom::ImeTextSpanThickness thickness,
    ui::mojom::ImeTextSpanUnderlineStyle underline_style,
    Color text_color,
    Color background_color) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(range,
                    [underline_color, thickness, underline_style, text_color,
                     background_color](int start_offset, int end_offset) {
                      return MakeGarbageCollected<CompositionMarker>(
                          start_offset, end_offset, underline_color, thickness,
                          underline_style, text_color, background_color);
                    });
}

void DocumentMarkerController::AddActiveSuggestionMarker(
    const EphemeralRange& range,
    Color underline_color,
    ui::mojom::ImeTextSpanThickness thickness,
    ui::mojom::ImeTextSpanUnderlineStyle underline_style,
    Color text_color,
    Color background_color) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(range,
                    [underline_color, thickness, underline_style, text_color,
                     background_color](int start_offset, int end_offset) {
                      return MakeGarbageCollected<ActiveSuggestionMarker>(
                          start_offset, end_offset, underline_color, thickness,
                          underline_style, text_color, background_color);
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

void DocumentMarkerController::AddCustomHighlightMarker(
    const EphemeralRange& range,
    const String& highlight_name,
    const Member<Highlight> highlight) {
  DCHECK(!document_->NeedsLayoutTreeUpdate());
  AddMarkerInternal(
      range, [highlight_name, highlight](int start_offset, int end_offset) {
        return MakeGarbageCollected<CustomHighlightMarker>(
            start_offset, end_offset, highlight_name, highlight);
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
    DCHECK(!markers_.empty());
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
    base::FunctionRef<DocumentMarker*(int, int)> create_marker_from_offsets,
    const TextIteratorBehavior& iterator_behavior) {
  DocumentMarkerGroup* new_marker_group =
      MakeGarbageCollected<DocumentMarkerGroup>();
  for (TextIterator marked_text(range.StartPosition(), range.EndPosition(),
                                iterator_behavior);
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
    new_marker_group->Set(new_marker, text_node);
    marker_groups_.insert(new_marker, new_marker_group);
  }
}

void DocumentMarkerController::AddMarkerToNode(const Text& text,
                                               DocumentMarker* new_marker) {
  DCHECK_GE(text.length(), new_marker->EndOffset());
  possibly_existing_marker_types_ = possibly_existing_marker_types_.Add(
      DocumentMarker::MarkerTypes(new_marker->GetType()));
  SetDocument(document_);

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
  DCHECK(!markers_.empty());

  MarkerLists* const src_markers = FindMarkers(&src_node);
  if (!src_markers)
    return;

  auto& dst_marker_entry =
      markers_.insert(&dst_node, nullptr).stored_value->value;
  if (!dst_marker_entry) {
    dst_marker_entry = MakeGarbageCollected<MarkerLists>(
        DocumentMarker::kMarkerTypeIndexesCount);
  }
  MarkerLists* const dst_markers = dst_marker_entry;

  bool doc_dirty = false;
  for (DocumentMarker::MarkerType type : DocumentMarker::MarkerTypes::All()) {
    DocumentMarkerList* const src_list = ListForType(src_markers, type);
    if (!src_list)
      continue;

    if (!ListForType(dst_markers, type))
      ListForType(dst_markers, type) = CreateListForType(type);

    DocumentMarkerList* const dst_list = ListForType(dst_markers, type);
    if (src_list->MoveMarkers(length, dst_list)) {
      doc_dirty = true;
      for (const auto& marker : dst_list->GetMarkers()) {
        auto it = marker_groups_.find(marker);
        if (it != marker_groups_.end())
          it->value->Set(marker, &dst_node);
      }
    }
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
  DCHECK(!(markers_.empty()));

  MarkerLists* const markers = FindMarkers(&text);
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

    const unsigned end_offset = start_offset + length;
    for (const Member<DocumentMarker>& marker : list->GetMarkers()) {
      if (marker->EndOffset() > start_offset &&
          marker->StartOffset() < end_offset) {
        auto it = marker_groups_.find(marker);
        if (it != marker_groups_.end()) {
          it->value->Erase(marker);
          marker_groups_.erase(marker);
        }
      }
    }
    if (list->RemoveMarkers(start_offset, length))
      doc_dirty = true;

    if (list->IsEmpty()) {
      ListForType(markers, type) = nullptr;
      ++empty_lists_count;
    }
  }

  if (empty_lists_count == DocumentMarker::kMarkerTypeIndexesCount) {
    markers_.erase(&text);
    if (markers_.empty()) {
      possibly_existing_marker_types_ = DocumentMarker::MarkerTypes();
      SetDocument(nullptr);
    }
  }

  if (!doc_dirty)
    return;

  InvalidatePaintForNode(text);
}

DocumentMarkerController::MarkerLists* DocumentMarkerController::FindMarkers(
    const Text* key) const {
  auto it = markers_.find(key);
  if (it != markers_.end()) {
    DCHECK(it->value);
    return it->value;
  }
  return nullptr;
}

DocumentMarker* DocumentMarkerController::FirstMarkerAroundPosition(
    const PositionInFlatTree& position,
    DocumentMarker::MarkerTypes types) {
  if (position.IsNull())
    return nullptr;
  const PositionInFlatTree& start = SearchAroundPositionStart(position);
  const PositionInFlatTree& end = SearchAroundPositionEnd(position);

  if (start > end) {
    // TODO(crbug/1114021): Investigate why this might happen.
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

  auto* text_node = DynamicTo<Text>(start_container);
  if (!text_node)
    return nullptr;

  const unsigned start_offset =
      range.StartPosition().ComputeOffsetInContainerNode();
  const unsigned end_offset =
      start_container == end_container
          ? range.EndPosition().ComputeOffsetInContainerNode()
          : text_node->length();

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

  MarkerLists* const markers = FindMarkers(&node);
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

DocumentMarkerGroup* DocumentMarkerController::FirstMarkerGroupAroundPosition(
    const PositionInFlatTree& position,
    DocumentMarker::MarkerTypes types) {
  return GetMarkerGroupForMarker(FirstMarkerAroundPosition(position, types));
}

DocumentMarkerGroup*
DocumentMarkerController::FirstMarkerGroupIntersectingEphemeralRange(
    const EphemeralRange& range,
    DocumentMarker::MarkerTypes types) {
  return GetMarkerGroupForMarker(
      FirstMarkerIntersectingEphemeralRange(range, types));
}

DocumentMarkerGroup*
DocumentMarkerController::FirstMarkerGroupIntersectingOffsetRange(
    const Text& node,
    unsigned start_offset,
    unsigned end_offset,
    DocumentMarker::MarkerTypes types) {
  return GetMarkerGroupForMarker(FirstMarkerIntersectingOffsetRange(
      node, start_offset, end_offset, types));
}

DocumentMarkerGroup* DocumentMarkerController::GetMarkerGroupForMarker(
    const DocumentMarker* marker) {
  if (marker) {
    auto it = marker_groups_.find(marker);
    if (it != marker_groups_.end())
      return it->value;
  }
  return nullptr;
}

HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>
DocumentMarkerController::MarkersAroundPosition(
    const PositionInFlatTree& position,
    DocumentMarker::MarkerTypes types) {
  HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>
      node_marker_pairs;

  if (position.IsNull())
    return node_marker_pairs;

  if (!PossiblyHasMarkers(types))
    return node_marker_pairs;

  const PositionInFlatTree& start = SearchAroundPositionStart(position);
  const PositionInFlatTree& end = SearchAroundPositionEnd(position);

  if (start > end) {
    // TODO(crbug/1114021): Investigate why this might happen.
    NOTREACHED() << "|start| should be before |end|.";
    return node_marker_pairs;
  }

  const Node* const start_node = start.ComputeContainerNode();
  const unsigned start_offset = start.ComputeOffsetInContainerNode();
  const Node* const end_node = end.ComputeContainerNode();
  const unsigned end_offset = end.ComputeOffsetInContainerNode();

  for (const Node& node : EphemeralRangeInFlatTree(start, end).Nodes()) {
    auto* text_node = DynamicTo<Text>(node);
    if (!text_node)
      continue;

    MarkerLists* const marker_lists = FindMarkers(text_node);
    if (!marker_lists)
      continue;

    const unsigned start_range_offset = node == start_node ? start_offset : 0;
    const unsigned end_range_offset =
        node == end_node ? end_offset : text_node->length();

    // Minor optimization: if we have an empty range at a node boundary, it
    // doesn't fall in the interior of any marker.
    if (start_range_offset == 0 && end_range_offset == 0)
      continue;
    const unsigned node_length = To<CharacterData>(node).length();
    if (start_range_offset == node_length && end_range_offset == node_length)
      continue;

    for (DocumentMarker::MarkerType type : types) {
      const DocumentMarkerList* const list = ListForType(marker_lists, type);
      if (!list)
        continue;

      const DocumentMarkerVector& marker_vector =
          list->MarkersIntersectingRange(start_range_offset, end_range_offset);

      for (DocumentMarker* marker : marker_vector)
        node_marker_pairs.push_back(std::make_pair(&To<Text>(node), marker));
    }
  }
  return node_marker_pairs;
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
    MarkerLists* const markers = FindMarkers(text_node);
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

  MarkerLists* markers = FindMarkers(&text);
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

DocumentMarkerVector
DocumentMarkerController::CustomHighlightMarkersNotOverlapping(
    const Text& text) const {
  // Fix overlapping CustomHighlightMarkers that share the same highlight name
  // so their intersections are not painted twice. Note:
  // DocumentMarkerController::MarkersFor() returns markers sorted by start
  // offset.
  DocumentMarkerVector custom_highlight_markers = MarkersFor(
      text, DocumentMarker::MarkerTypes(DocumentMarker::kCustomHighlight));
  DocumentMarkerVector result{};
  using NameToCustomHighlightMarkerMap =
      HashMap<String, Member<CustomHighlightMarker>, StringHash>;
  NameToCustomHighlightMarkerMap name_to_last_custom_highlight_marker_seen;

  for (const auto& current_marker : custom_highlight_markers) {
    CustomHighlightMarker* current_custom_highlight_marker =
        To<CustomHighlightMarker>(current_marker.Get());

    NameToCustomHighlightMarkerMap::AddResult insert_result =
        name_to_last_custom_highlight_marker_seen.insert(
            current_custom_highlight_marker->GetHighlightName(),
            current_custom_highlight_marker);

    if (!insert_result.is_new_entry) {
      CustomHighlightMarker* stored_custom_highlight_marker =
          insert_result.stored_value->value;
      if (current_custom_highlight_marker->StartOffset() >=
          stored_custom_highlight_marker->EndOffset()) {
        // Markers don't intersect, so the stored one is fine to be painted.
        result.push_back(stored_custom_highlight_marker);
        insert_result.stored_value->value = current_custom_highlight_marker;
      } else {
        // Markers overlap, so expand the stored marker to cover both and
        // discard the current one.
        stored_custom_highlight_marker->SetEndOffset(
            std::max(stored_custom_highlight_marker->EndOffset(),
                     current_custom_highlight_marker->EndOffset()));
      }
    }
  }

  for (const auto& name_to_custom_highlight_marker_iterator :
       name_to_last_custom_highlight_marker_seen) {
    result.push_back(name_to_custom_highlight_marker_iterator.value.Get());
  }

  return result;
}

DocumentMarkerVector DocumentMarkerController::ComputeMarkersToPaint(
    const Text& text) const {
  HighlightRegistry* highlight_registry =
      document_->domWindow()->Supplementable<LocalDOMWindow>::
          RequireSupplement<HighlightRegistry>();
  DocumentMarker::MarkerTypes excluded_highlight_pseudos =
      RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled()
          ? DocumentMarker::MarkerTypes::HighlightPseudos()
          : DocumentMarker::MarkerTypes();
  DocumentMarkerVector markers_to_paint{};

  if (!RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled()) {
    DocumentMarkerVector custom_highlight_markers =
        CustomHighlightMarkersNotOverlapping(text);
    std::sort(custom_highlight_markers.begin(), custom_highlight_markers.end(),
              [highlight_registry](const Member<DocumentMarker>& marker1,
                                   const Member<DocumentMarker>& marker2) {
                auto* custom1 = To<CustomHighlightMarker>(marker1.Get());
                auto* custom2 = To<CustomHighlightMarker>(marker2.Get());
                return highlight_registry->CompareOverlayStackingPosition(
                           custom1->GetHighlightName(), custom1->GetHighlight(),
                           custom2->GetHighlightName(),
                           custom2->GetHighlight()) ==
                       HighlightRegistry::OverlayStackingPosition::
                           kOverlayStackingPositionBelow;
              });
    markers_to_paint = custom_highlight_markers;
  }

  // We don't render composition or spelling markers that overlap suggestion
  // markers.
  // Note: DocumentMarkerController::MarkersFor() returns markers sorted by
  // start offset.
  const DocumentMarkerVector& suggestion_markers =
      MarkersFor(text, DocumentMarker::MarkerTypes::Suggestion());
  if (suggestion_markers.empty()) {
    // If there are no suggestion markers, we can return early as a minor
    // performance optimization.
    markers_to_paint.AppendVector(MarkersFor(
        text, DocumentMarker::MarkerTypes::AllBut(
                  DocumentMarker::MarkerTypes(DocumentMarker::kSuggestion |
                                              DocumentMarker::kCustomHighlight))
                  .Subtract(excluded_highlight_pseudos)));
    return markers_to_paint;
  }

  const DocumentMarkerVector& markers_overridden_by_suggestion_markers =
      MarkersFor(text,
                 DocumentMarker::MarkerTypes(DocumentMarker::kComposition |
                                             DocumentMarker::kSpelling)
                     .Subtract(excluded_highlight_pseudos));

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
      text,
      DocumentMarker::MarkerTypes::AllBut(
          DocumentMarker::MarkerTypes(
              DocumentMarker::kComposition | DocumentMarker::kSpelling |
              DocumentMarker::kSuggestion | DocumentMarker::kCustomHighlight))
          .Subtract(excluded_highlight_pseudos)));

  return markers_to_paint;
}

bool DocumentMarkerController::PossiblyHasTextMatchMarkers() const {
  return PossiblyHasMarkers(DocumentMarker::kTextMatch);
}

Vector<gfx::Rect> DocumentMarkerController::LayoutRectsForTextMatchMarkers() {
  DCHECK(!document_->View()->NeedsLayout());
  DCHECK(!document_->NeedsLayoutTreeUpdate());

  Vector<gfx::Rect> result;

  if (!PossiblyHasMarkers(DocumentMarker::kTextMatch))
    return result;
  DCHECK(!(markers_.empty()));

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

void DocumentMarkerController::DidProcessMarkerMap(const LivenessBroker&) {
  if (markers_.empty())
    Clear();
}

void DocumentMarkerController::Trace(Visitor* visitor) const {
  // Note: To make |DidProcessMarkerMap()| called after weak members callback
  // of |markers_|, we should register it before tracing |markers_|.
  visitor->template RegisterWeakCallbackMethod<
      DocumentMarkerController, &DocumentMarkerController::DidProcessMarkerMap>(
      this);
  visitor->Trace(markers_);
  visitor->Trace(marker_groups_);
  visitor->Trace(document_);
  SynchronousMutationObserver::Trace(visitor);
}

void DocumentMarkerController::RemoveMarkersForNode(
    const Text& text,
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!markers_.empty());

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

void DocumentMarkerController::RemoveSuggestionMarkerByType(
    const EphemeralRangeInFlatTree& range,
    const SuggestionMarker::SuggestionType& type) {
  // MarkersIntersectingRange() might be expensive. In practice, we hope we will
  // only check one node for the range.
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      node_marker_pairs = MarkersIntersectingRange(
          range, DocumentMarker::MarkerTypes::Suggestion());
  for (const auto& node_marker_pair : node_marker_pairs) {
    const Text& text = *node_marker_pair.first;
    DocumentMarkerList* const list =
        ListForType(markers_.at(&text), DocumentMarker::kSuggestion);
    // RemoveMarkerByType() might be expensive. In practice, we have at most
    // one suggestion marker needs to be removed.
    To<SuggestionMarkerListImpl>(list)->RemoveMarkerByType(type);
    InvalidatePaintForNode(text);
  }
}

void DocumentMarkerController::RemoveSuggestionMarkerByType(
    const SuggestionMarker::SuggestionType& type) {
  if (!PossiblyHasMarkers(DocumentMarker::kSuggestion))
    return;
  DCHECK(!markers_.empty());

  for (const auto& node_markers : markers_) {
    MarkerLists* markers = node_markers.value;
    DocumentMarkerList* const list =
        ListForType(markers, DocumentMarker::kSuggestion);
    if (!list)
      continue;
    if (To<SuggestionMarkerListImpl>(list)->RemoveMarkerByType(type)) {
      InvalidatePaintForNode(*node_markers.key);
      return;
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
  DCHECK(!markers_.empty());

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
  SetDocument(nullptr);
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
    if (markers_.empty()) {
      possibly_existing_marker_types_ = DocumentMarker::MarkerTypes();
      SetDocument(nullptr);
    }
  }
}

void DocumentMarkerController::RepaintMarkers(
    DocumentMarker::MarkerTypes marker_types) {
  if (!PossiblyHasMarkers(marker_types))
    return;
  DCHECK(!markers_.empty());

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

  DCHECK(!markers_.empty());

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
  MarkerLists* markers = FindMarkers(&text);
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
  DCHECK(!markers_.empty());
  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return;
  MarkerLists* markers = FindMarkers(text_node);
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
void ShowDocumentMarkers(const blink::DocumentMarkerController* controller) {
  if (controller)
    controller->ShowMarkers();
}
#endif
