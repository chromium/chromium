// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/iterators/text_iterator_behavior.h"

namespace blink {

TextIteratorBehavior::Builder::Builder(const TextIteratorBehavior& behavior)
    : behavior_(behavior) {}

TextIteratorBehavior::Builder::Builder() = default;
TextIteratorBehavior::Builder::~Builder() = default;

TextIteratorBehavior TextIteratorBehavior::Builder::Build() {
  return behavior_;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetDoesNotBreakAtReplacedElement(bool value) {
  behavior_.values_.bits.does_not_break_at_replaced_element = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsCharactersBetweenAllVisiblePositions(
    bool value) {
  behavior_.values_.bits.emits_characters_between_all_visible_positions = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsImageAltText(bool value) {
  behavior_.values_.bits.emits_image_alt_text = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsSpaceForNbsp(bool value) {
  behavior_.values_.bits.emits_space_for_nbsp = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsObjectReplacementCharacter(bool value) {
  behavior_.values_.bits.emits_object_replacement_character = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsOriginalText(bool value) {
  behavior_.values_.bits.emits_original_text = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsSmallXForTextSecurity(bool value) {
  behavior_.values_.bits.emits_small_x_for_text_security = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEntersOpenShadowRoots(bool value) {
  behavior_.values_.bits.enters_open_shadow_roots = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEntersTextControls(bool value) {
  behavior_.values_.bits.enters_text_controls = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetExcludeAutofilledValue(bool value) {
  behavior_.values_.bits.exclude_autofilled_value = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetForSelectionToString(bool value) {
  behavior_.values_.bits.for_selection_to_string = value;
  return *this;
}

TextIteratorBehavior::Builder& TextIteratorBehavior::Builder::SetForWindowFind(
    bool value) {
  behavior_.values_.bits.for_window_find = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetIgnoresStyleVisibility(bool value) {
  behavior_.values_.bits.ignores_style_visibility = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetStopsOnFormControls(bool value) {
  behavior_.values_.bits.stops_on_form_controls = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetDoesNotEmitSpaceBeyondRangeEnd(bool value) {
  behavior_.values_.bits.does_not_emit_space_beyond_range_end = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetSkipsUnselectableContent(bool value) {
  behavior_.values_.bits.skips_unselectable_content = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetSuppressesExtraNewlineEmission(bool value) {
  behavior_.values_.bits.suppresses_newline_emission = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetIgnoresDisplayLock(bool value) {
  behavior_.values_.bits.ignores_display_lock = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetEmitsPunctuationForReplacedElements(
    bool value) {
  behavior_.values_.bits.emits_punctuation_for_replaced_elements = value;
  return *this;
}

TextIteratorBehavior::Builder&
TextIteratorBehavior::Builder::SetIgnoresCSSTextTransforms(bool value) {
  behavior_.values_.bits.ignores_css_text_transforms = value;
  return *this;
}

// -
TextIteratorBehavior::TextIteratorBehavior(const TextIteratorBehavior& other) =
    default;

TextIteratorBehavior::TextIteratorBehavior() {
  values_.all = 0;
}

bool TextIteratorBehavior::operator==(const TextIteratorBehavior& other) const {
  return values_.all == other.values_.all;
}

bool TextIteratorBehavior::operator!=(const TextIteratorBehavior& other) const {
  return !operator==(other);
}

// static
TextIteratorBehavior
TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior() {
  return TextIteratorBehavior::Builder()
      .SetEmitsObjectReplacementCharacter(true)
      .Build();
}

// static
TextIteratorBehavior TextIteratorBehavior::IgnoresStyleVisibilityBehavior() {
  return TextIteratorBehavior::Builder()
      .SetIgnoresStyleVisibility(true)
      .Build();
}

// static
TextIteratorBehavior TextIteratorBehavior::DefaultRangeLengthBehavior() {
  return TextIteratorBehavior::Builder()
      .SetEmitsObjectReplacementCharacter(true)
      .Build();
}

// static
TextIteratorBehavior
TextIteratorBehavior::AllVisiblePositionsRangeLengthBehavior() {
  return TextIteratorBehavior::Builder()
      .SetEmitsObjectReplacementCharacter(true)
      .SetEmitsCharactersBetweenAllVisiblePositions(true)
      .Build();
}

// static
TextIteratorBehavior
TextIteratorBehavior::NoTrailingSpaceRangeLengthBehavior() {
  return TextIteratorBehavior::Builder()
      .SetEmitsObjectReplacementCharacter(true)
      .SetDoesNotEmitSpaceBeyondRangeEnd(true)
      .Build();
}

}  // namespace blink
