// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"

namespace blink {

SuggestionMarkerProperties::SuggestionMarkerProperties() = default;
SuggestionMarkerProperties::SuggestionMarkerProperties(
    const SuggestionMarkerProperties& other) = default;
SuggestionMarkerProperties& SuggestionMarkerProperties::operator=(
    const SuggestionMarkerProperties& other) = default;
SuggestionMarkerProperties::Builder::Builder() = default;

SuggestionMarkerProperties::Builder::Builder(
    const SuggestionMarkerProperties& data) {
  data_ = data;
}

SuggestionMarkerProperties SuggestionMarkerProperties::Builder::Build() const {
  return data_;
}

SuggestionMarkerProperties::Builder&
SuggestionMarkerProperties::Builder::SetType(
    SuggestionMarker::SuggestionType type) {
  data_.type_ = type;
  return *this;
}

SuggestionMarkerProperties::Builder&
SuggestionMarkerProperties::Builder::SetRemoveOnFinishComposing(
    bool remove_on_finish_composing) {
  data_.remove_on_finish_composing_ =
      remove_on_finish_composing
          ? SuggestionMarker::RemoveOnFinishComposing::kRemove
          : SuggestionMarker::RemoveOnFinishComposing::kDoNotRemove;
  return *this;
}

SuggestionMarkerProperties::Builder&
SuggestionMarkerProperties::Builder::SetSuggestions(
    const Vector<String>& suggestions) {
  data_.suggestions_ = suggestions;
  return *this;
}

SuggestionMarkerProperties::Builder&
SuggestionMarkerProperties::Builder::SetHighlightColor(Color highlight_color) {
  data_.highlight_color_ = highlight_color;
  return *this;
}

SuggestionMarkerProperties::Builder&
SuggestionMarkerProperties::Builder::SetUnderlineColor(Color underline_color) {
  data_.underline_color_ = underline_color;
  return *this;
}

SuggestionMarkerProperties::Builder&
SuggestionMarkerProperties::Builder::SetBackgroundColor(
    Color background_color) {
  data_.background_color_ = background_color;
  return *this;
}

SuggestionMarkerProperties::Builder&
SuggestionMarkerProperties::Builder::SetThickness(
    ui::mojom::ImeTextSpanThickness thickness) {
  data_.thickness_ = thickness;
  return *this;
}

SuggestionMarkerProperties::Builder&
SuggestionMarkerProperties::Builder::SetUnderlineStyle(
    ui::mojom::ImeTextSpanUnderlineStyle underline_style) {
  data_.underline_style_ = underline_style;
  return *this;
}

SuggestionMarkerProperties::Builder&
SuggestionMarkerProperties::Builder::SetTextColor(Color text_color) {
  data_.text_color_ = text_color;
  return *this;
}

}  // namespace blink
