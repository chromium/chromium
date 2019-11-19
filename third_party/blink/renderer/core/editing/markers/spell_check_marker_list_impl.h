// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_SPELL_CHECK_MARKER_LIST_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_SPELL_CHECK_MARKER_LIST_IMPL_H_

#include "third_party/blink/renderer/core/editing/markers/document_marker_list.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// Nearly-complete implementation of DocumentMarkerList for Spelling or Grammar
// markers (subclassed by SpellingMarkerListImpl and GrammarMarkerListImpl to
// implement the MarkerType() method). Markers with touching endpoints are
// merged on insert. Markers are kept sorted by start offset in order to be able
// to do this efficiently.
class CORE_EXPORT SpellCheckMarkerListImpl : public DocumentMarkerList {
 public:
  // DocumentMarkerList implementations
  bool IsEmpty() const final;

  void Add(DocumentMarker*) final;
  void Clear() final;

  const HeapVector<Member<DocumentMarker>>& GetMarkers() const final;
  DocumentMarker* FirstMarkerIntersectingRange(unsigned start_offset,
                                               unsigned end_offset) const final;
  HeapVector<Member<DocumentMarker>> MarkersIntersectingRange(
      unsigned start_offset,
      unsigned end_offset) const final;

  bool MoveMarkers(int length, DocumentMarkerList* dst_list) final;
  bool RemoveMarkers(unsigned start_offset, int length) final;
  bool ShiftMarkers(const String& node_text,
                    unsigned offset,
                    unsigned old_length,
                    unsigned new_length) final;

  void Trace(Visitor*) override;

  // SpellCheckMarkerListImpl-specific
  // Returns true if a marker was removed, false otherwise.
  bool RemoveMarkersUnderWords(const String& node_text,
                               const Vector<String>& words);

 protected:
  SpellCheckMarkerListImpl() = default;

 private:
  HeapVector<Member<DocumentMarker>> markers_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckMarkerListImpl);
};

template <>
struct DowncastTraits<SpellCheckMarkerListImpl> {
  static bool AllowFrom(const DocumentMarkerList& list) {
    return list.MarkerType() == DocumentMarker::kSpelling ||
           list.MarkerType() == DocumentMarker::kGrammar;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_SPELL_CHECK_MARKER_LIST_IMPL_H_
