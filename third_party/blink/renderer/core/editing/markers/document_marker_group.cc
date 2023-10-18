// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/document_marker_group.h"

namespace blink {

void DocumentMarkerGroup::Trace(Visitor* visitor) const {
  visitor->Trace(marker_text_map_);
}

Position DocumentMarkerGroup::StartPosition() const {
  const auto start_marker_text = std::min_element(
      marker_text_map_.begin(), marker_text_map_.end(),
      [](const auto& marker1, const auto& marker2) {
        return Position(marker1.value, marker1.key->StartOffset()) <
               Position(marker2.value, marker2.key->StartOffset());
      });
  return Position(start_marker_text->value,
                  start_marker_text->key->StartOffset());
}

Position DocumentMarkerGroup::EndPosition() const {
  const auto end_marker_text = std::max_element(
      marker_text_map_.begin(), marker_text_map_.end(),
      [](const auto& marker1, const auto& marker2) {
        return Position(marker1.value, marker1.key->EndOffset()) <
               Position(marker2.value, marker2.key->EndOffset());
      });
  return Position(end_marker_text->value, end_marker_text->key->EndOffset());
}

const DocumentMarker* DocumentMarkerGroup::GetMarkerForText(
    const Text* text) const {
  for (const auto& marker_text : marker_text_map_) {
    if (marker_text.value == text) {
      return marker_text.key.Get();
    }
  }
  return nullptr;
}

}  // namespace blink