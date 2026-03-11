// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester_helper.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/grammar_marker.h"
#include "third_party/blink/renderer/core/editing/markers/spelling_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

bool ShouldSendSpellingMarkersInfo() {
#if BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(
      blink::features::kAndroidSpellcheckFullApiBlink);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

DocumentMarkerVector GetSpellingMarkersFromRange(const Document& document,
                                                 ContainerNode* container,
                                                 const EphemeralRange& range) {
  if (container == nullptr) {
    return {};
  }

  DocumentMarkerVector markers;

  // Compute the start index of the beginning of the range with respect to the
  // container.
  size_t range_start_offset = PlainTextRange::Create(*container, range).Start();
  EphemeralRangeInFlatTree range_in_flat_tree =
      ToEphemeralRangeInFlatTree(range);

  for (const auto& pair : document.Markers().MarkersIntersectingRange(
           range_in_flat_tree, DocumentMarker::MarkerTypes(
                                   DocumentMarker::MarkerType::kSpelling |
                                   DocumentMarker::MarkerType::kGrammar |
                                   DocumentMarker::MarkerType::kSuggestion))) {
    DocumentMarker* marker = pair.second.Get();
    PlainTextRange marker_in_container = PlainTextRange::Create(
        *container, EphemeralRange(Position(pair.first, marker->StartOffset()),
                                   Position(pair.first, marker->EndOffset())));

    size_t marker_in_container_start_offset = marker_in_container.Start();
    size_t marker_in_container_end_offset = marker_in_container.End();

    if (marker_in_container_start_offset < range_start_offset ||
        marker_in_container_end_offset < range_start_offset) {
      continue;
    }

    size_t start_offset = marker_in_container_start_offset - range_start_offset;
    size_t end_offset = marker_in_container_end_offset - range_start_offset;

    if (marker->GetType() == DocumentMarker::MarkerType::kSpelling ||
        (IsA<SuggestionMarker>(marker) &&
         To<SuggestionMarker>(marker)->IsMisspelling())) {
      // Some spell check services (e.g. Gboard) only require the start and end
      // offset.
      markers.push_back(MakeGarbageCollected<SpellingMarker>(
          start_offset, end_offset, g_empty_string));
    } else if (marker->GetType() == DocumentMarker::MarkerType::kGrammar ||
               (IsA<SuggestionMarker>(marker) &&
                To<SuggestionMarker>(marker)->IsGrammarError())) {
      // Some spell check services (e.g. Gboard) only require the start and end
      // offset.
      markers.push_back(MakeGarbageCollected<GrammarMarker>(
          start_offset, end_offset, g_empty_string));
    }
  }
  return markers;
}

}  // namespace blink
