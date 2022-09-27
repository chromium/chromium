// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"

#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"

namespace blink {

int32_t SuggestionMarker::current_tag_ = 0;

SuggestionMarker::SuggestionMarker(unsigned start_offset,
                                   unsigned end_offset,
                                   const SuggestionMarkerProperties& properties)
    : StyleableMarker(start_offset,
                      end_offset,
                      properties.UnderlineColor(),
                      properties.Thickness(),
                      properties.UnderlineStyle(),
                      properties.TextColor(),
                      properties.BackgroundColor()),
      tag_(NextTag()),
      suggestions_(properties.Suggestions()),
      suggestion_type_(properties.Type()),
      remove_on_finish_composing_(properties.RemoveOnFinishComposing()),
      suggestion_highlight_color_(properties.HighlightColor()) {
  DCHECK_GT(tag_, 0);
}

int32_t SuggestionMarker::Tag() const {
  return tag_;
}

SuggestionMarker::SuggestionType SuggestionMarker::GetSuggestionType() const {
  return suggestion_type_;
}

DocumentMarker::MarkerType SuggestionMarker::GetType() const {
  return DocumentMarker::kSuggestion;
}

const Vector<String>& SuggestionMarker::Suggestions() const {
  return suggestions_;
}

bool SuggestionMarker::IsMisspelling() const {
  return suggestion_type_ == SuggestionType::kMisspelling;
}

bool SuggestionMarker::NeedsRemovalOnFinishComposing() const {
  return remove_on_finish_composing_ == RemoveOnFinishComposing::kRemove;
}

Color SuggestionMarker::SuggestionHighlightColor() const {
  return suggestion_highlight_color_;
}

void SuggestionMarker::SetSuggestion(uint32_t suggestion_index,
                                     const String& new_suggestion) {
  DCHECK_LT(suggestion_index, suggestions_.size());
  suggestions_[suggestion_index] = new_suggestion;
}

// static
int32_t SuggestionMarker::NextTag() {
  return ++current_tag_;
}

}  // namespace blink
