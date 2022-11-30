// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/set_selection_options.h"

namespace blink {

SetSelectionOptions::SetSelectionOptions() = default;
SetSelectionOptions::SetSelectionOptions(const SetSelectionOptions& other) =
    default;
SetSelectionOptions& SetSelectionOptions::operator=(
    const SetSelectionOptions& other) = default;
SetSelectionOptions::Builder::Builder() = default;

SetSelectionOptions::Builder::Builder(const SetSelectionOptions& data) {
  data_ = data;
}

SetSelectionOptions SetSelectionOptions::Builder::Build() const {
  return data_;
}

SetSelectionOptions::Builder&
SetSelectionOptions::Builder::SetCursorAlignOnScroll(
    CursorAlignOnScroll align) {
  data_.cursor_align_on_scroll_ = align;
  return *this;
}

SetSelectionOptions::Builder&
SetSelectionOptions::Builder::SetDoNotClearStrategy(bool new_value) {
  data_.do_not_clear_strategy_ = new_value;
  return *this;
}

SetSelectionOptions::Builder& SetSelectionOptions::Builder::SetDoNotSetFocus(
    bool new_value) {
  data_.do_not_set_focus_ = new_value;
  return *this;
}

SetSelectionOptions::Builder& SetSelectionOptions::Builder::SetGranularity(
    TextGranularity new_value) {
  data_.granularity_ = new_value;
  return *this;
}

SetSelectionOptions::Builder& SetSelectionOptions::Builder::SetSetSelectionBy(
    SetSelectionBy new_value) {
  data_.set_selection_by_ = new_value;
  return *this;
}

SetSelectionOptions::Builder&
SetSelectionOptions::Builder::SetShouldClearTypingStyle(bool new_value) {
  data_.should_clear_typing_style_ = new_value;
  return *this;
}

SetSelectionOptions::Builder&
SetSelectionOptions::Builder::SetShouldCloseTyping(bool new_value) {
  data_.should_close_typing_ = new_value;
  return *this;
}

SetSelectionOptions::Builder& SetSelectionOptions::Builder::SetShouldShowHandle(
    bool new_value) {
  data_.should_show_handle_ = new_value;
  return *this;
}

SetSelectionOptions::Builder&
SetSelectionOptions::Builder::SetShouldShrinkNextTap(bool new_value) {
  data_.should_shrink_next_tap_ = new_value;
  return *this;
}

SetSelectionOptions::Builder& SetSelectionOptions::Builder::SetIsDirectional(
    bool new_value) {
  data_.is_directional_ = new_value;
  return *this;
}
}  // namespace blink
