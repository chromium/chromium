// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_SUGGESTION_MARKER_PROPERTIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_SUGGESTION_MARKER_PROPERTIES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/markers/styleable_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"

using ui::mojom::ImeTextSpanThickness;

namespace blink {

// This class is used to pass parameters to
// |DocumentMarkerController::AddSuggestionMarker()|.
class CORE_EXPORT SuggestionMarkerProperties final {
  STACK_ALLOCATED();

 public:
  class CORE_EXPORT Builder;

  SuggestionMarkerProperties(const SuggestionMarkerProperties&);
  SuggestionMarkerProperties();

  SuggestionMarker::SuggestionType Type() const { return type_; }
  SuggestionMarker::RemoveOnFinishComposing RemoveOnFinishComposing() const {
    return remove_on_finish_composing_;
  }
  Vector<String> Suggestions() const { return suggestions_; }
  Color HighlightColor() const { return highlight_color_; }
  Color UnderlineColor() const { return underline_color_; }
  Color BackgroundColor() const { return background_color_; }
  ImeTextSpanThickness Thickness() const { return thickness_; }

 private:
  SuggestionMarker::SuggestionType type_ =
      SuggestionMarker::SuggestionType::kNotMisspelling;
  SuggestionMarker::RemoveOnFinishComposing remove_on_finish_composing_ =
      SuggestionMarker::RemoveOnFinishComposing::kDoNotRemove;
  Vector<String> suggestions_;
  Color highlight_color_ = Color::kTransparent;
  Color underline_color_ = Color::kTransparent;
  Color background_color_ = Color::kTransparent;
  ImeTextSpanThickness thickness_ = ImeTextSpanThickness::kThin;
};

// This class is used for building SuggestionMarkerProperties objects.
class CORE_EXPORT SuggestionMarkerProperties::Builder final {
  STACK_ALLOCATED();

 public:
  explicit Builder(const SuggestionMarkerProperties&);
  Builder();

  SuggestionMarkerProperties Build() const;

  Builder& SetType(SuggestionMarker::SuggestionType);
  Builder& SetRemoveOnFinishComposing(bool);
  Builder& SetSuggestions(const Vector<String>& suggestions);
  Builder& SetHighlightColor(Color);
  Builder& SetUnderlineColor(Color);
  Builder& SetBackgroundColor(Color);
  Builder& SetThickness(ImeTextSpanThickness);

 private:
  SuggestionMarkerProperties data_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_MARKERS_SUGGESTION_MARKER_PROPERTIES_H_
