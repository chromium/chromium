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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_DOCUMENT_MARKER_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_DOCUMENT_MARKER_CONTROLLER_H_

#include <utility>

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/composition_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DocumentMarkerList;
class SuggestionMarkerProperties;

class CORE_EXPORT DocumentMarkerController final
    : public GarbageCollected<DocumentMarkerController>,
      public SynchronousMutationObserver {
  USING_GARBAGE_COLLECTED_MIXIN(DocumentMarkerController);

 public:
  explicit DocumentMarkerController(Document&);

  void Clear();
  void AddSpellingMarker(const EphemeralRange&,
                         const String& description = g_empty_string);
  void AddGrammarMarker(const EphemeralRange&,
                        const String& description = g_empty_string);
  void AddTextMatchMarker(const EphemeralRange&, TextMatchMarker::MatchStatus);
  void AddCompositionMarker(const EphemeralRange&,
                            Color underline_color,
                            ui::mojom::ImeTextSpanThickness,
                            Color background_color);
  void AddActiveSuggestionMarker(const EphemeralRange&,
                                 Color underline_color,
                                 ui::mojom::ImeTextSpanThickness,
                                 Color background_color);
  void AddSuggestionMarker(const EphemeralRange&,
                           const SuggestionMarkerProperties&);
  void AddTextFragmentMarker(const EphemeralRange&);

  void MoveMarkers(const Text& src_node, int length, const Text& dst_node);

  void PrepareForDestruction();
  void RemoveMarkersInRange(const EphemeralRange&, DocumentMarker::MarkerTypes);
  void RemoveMarkersOfTypes(DocumentMarker::MarkerTypes);
  void RemoveMarkersForNode(
      const Text&,
      DocumentMarker::MarkerTypes = DocumentMarker::MarkerTypes::All());
  void RemoveSpellingMarkersUnderWords(const Vector<String>& words);
  void RemoveSuggestionMarkerByTag(const Text&, int32_t marker_tag);
  // Removes suggestion marker with |RemoveOnFinishComposing::kRemove|.
  void RemoveSuggestionMarkerInRangeOnFinish(const EphemeralRangeInFlatTree&);
  void RepaintMarkers(
      DocumentMarker::MarkerTypes = DocumentMarker::MarkerTypes::All());
  // Returns true if markers within a range are found.
  bool SetTextMatchMarkersActive(const EphemeralRange&, bool);
  // Returns true if markers within a range defined by a text node,
  // |start_offset| and |end_offset| are found.
  bool SetTextMatchMarkersActive(const Text&,
                                 unsigned start_offset,
                                 unsigned end_offset,
                                 bool);

  // TODO(rlanday): can these methods for retrieving markers be consolidated
  // without hurting efficiency?

  // If the given position is either at the boundary or inside a word, expands
  // the position to the surrounding word and then looks for a marker having the
  // specified type. If the position is neither at the boundary or inside a
  // word, expands the position to cover the space between the end of the
  // previous and the start of the next words. If such a marker exists, this
  // method will return one of them (no guarantees are provided as to which
  // one). Otherwise, this method will return null.
  DocumentMarker* FirstMarkerAroundPosition(const PositionInFlatTree&,
                                            DocumentMarker::MarkerTypes);
  // Looks for a marker in the specified EphemeralRange of the specified type
  // whose interior has non-empty overlap with the bounds of the range.
  // If the range is collapsed, it uses FirstMarkerAroundPosition to expand the
  // range to the surrounding word.
  // If such a marker exists, this method will return one of them (no guarantees
  // are provided as to which one). Otherwise, this method will return null.
  DocumentMarker* FirstMarkerIntersectingEphemeralRange(
      const EphemeralRange&,
      DocumentMarker::MarkerTypes);
  // Looks for a marker in the specified node of the specified type whose
  // interior has non-empty overlap with the range [start_offset, end_offset].
  // If the range is collapsed, this looks for a marker containing the offset of
  // the collapsed range in its interior.
  // If such a marker exists, this method will return one of them (no guarantees
  // are provided as to which one). Otherwise, this method will return null.
  DocumentMarker* FirstMarkerIntersectingOffsetRange(
      const Text&,
      unsigned start_offset,
      unsigned end_offset,
      DocumentMarker::MarkerTypes);
  // Return all markers of the specified types whose interiors have non-empty
  // overlap with the specified range. Note that the range can be collapsed, in
  // in which case markers containing the position in their interiors are
  // returned.
  HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>
  MarkersIntersectingRange(const EphemeralRangeInFlatTree&,
                           DocumentMarker::MarkerTypes);
  DocumentMarkerVector MarkersFor(
      const Text&,
      DocumentMarker::MarkerTypes = DocumentMarker::MarkerTypes::All()) const;
  DocumentMarkerVector Markers() const;
  DocumentMarkerVector ComputeMarkersToPaint(const Text&) const;

  bool PossiblyHasTextMatchMarkers() const;
  Vector<IntRect> LayoutRectsForTextMatchMarkers();
  void InvalidateRectsForAllTextMatchMarkers();
  void InvalidateRectsForTextMatchMarkersInNode(const Text&);

  void Trace(Visitor*) override;

#if DCHECK_IS_ON()
  void ShowMarkers() const;
#endif

  // SynchronousMutationObserver
  // For performance, observer is only registered when
  // |possibly_existing_marker_types_| is non-zero.
  void DidUpdateCharacterData(CharacterData*,
                              unsigned offset,
                              unsigned old_length,
                              unsigned new_length) final;

 private:
  void AddMarkerInternal(
      const EphemeralRange&,
      std::function<DocumentMarker*(int, int)> create_marker_from_offsets);
  void AddMarkerToNode(const Text&, DocumentMarker*);

  using MarkerLists = HeapVector<Member<DocumentMarkerList>,
                                 DocumentMarker::kMarkerTypeIndexesCount>;
  using MarkerMap = HeapHashMap<WeakMember<const Text>, Member<MarkerLists>>;
  static Member<DocumentMarkerList>& ListForType(MarkerLists*,
                                                 DocumentMarker::MarkerType);
  bool PossiblyHasMarkers(DocumentMarker::MarkerTypes) const;
  bool PossiblyHasMarkers(DocumentMarker::MarkerType) const;
  void RemoveMarkersFromList(MarkerMap::iterator, DocumentMarker::MarkerTypes);
  void RemoveMarkers(TextIterator&, DocumentMarker::MarkerTypes);
  void RemoveMarkersInternal(const Text&,
                             unsigned start_offset,
                             int length,
                             DocumentMarker::MarkerTypes);

  // Called after weak processing of |markers_| is done.
  void DidProcessMarkerMap(const WeakCallbackInfo&);

  MarkerMap markers_;
  // Provide a quick way to determine whether a particular marker type is absent
  // without going through the map.
  DocumentMarker::MarkerTypes possibly_existing_marker_types_;
  const Member<Document> document_;

  DISALLOW_COPY_AND_ASSIGN(DocumentMarkerController);
};

}  // namespace blink

#if DCHECK_IS_ON()
void showDocumentMarkers(const blink::DocumentMarkerController*);
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_DOCUMENT_MARKER_CONTROLLER_H_
