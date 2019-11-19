// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_SUGGESTION_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_SUGGESTION_MARKER_H_

#include "third_party/blink/renderer/core/editing/markers/styleable_marker.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class SuggestionMarkerProperties;

// A subclass of StyleableMarker used to store information specific to
// suggestion markers (used to represent Android SuggestionSpans). In addition
// to the formatting information StyleableMarker holds, we also store a list of
// suggested replacements for the marked region of text. In addition, each
// SuggestionMarker is tagged with an integer so browser code can identify which
// SuggestionMarker a suggestion replace operation pertains to.
class CORE_EXPORT SuggestionMarker final : public StyleableMarker {
 public:
  enum class SuggestionType { kMisspelling, kNotMisspelling };
  enum class RemoveOnFinishComposing { kRemove, kDoNotRemove };

  SuggestionMarker(unsigned start_offset,
                   unsigned end_offset,
                   const SuggestionMarkerProperties&);

  // DocumentMarker implementations
  MarkerType GetType() const final;

  // SuggestionMarker-specific
  int32_t Tag() const;
  const Vector<String>& Suggestions() const;
  bool IsMisspelling() const;
  bool NeedsRemovalOnFinishComposing() const;
  Color SuggestionHighlightColor() const;

  // Replace the suggestion at suggestion_index with new_suggestion.
  void SetSuggestion(unsigned suggestion_index, const String& new_suggestion);

 private:
  static int32_t NextTag();

  static int32_t current_tag_;

  // We use a signed int for the tag since it's passed to Java (as an opaque
  // identifier), and Java does not support unsigned ints.
  const int32_t tag_;
  Vector<String> suggestions_;
  const SuggestionType suggestion_type_;
  const RemoveOnFinishComposing remove_on_finish_composing_;
  const Color suggestion_highlight_color_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionMarker);
};

template <>
struct DowncastTraits<SuggestionMarker> {
  static bool AllowFrom(const DocumentMarker& marker) {
    return marker.GetType() == DocumentMarker::kSuggestion;
  }
};

}  // namespace blink

#endif
