// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/trusted/browser_font_trusted.h"

#include <algorithm>

#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_BrowserFont_Trusted_1_0>() {
  return PPB_BROWSERFONT_TRUSTED_INTERFACE_1_0;
}

}  // namespace

// BrowserFontDescription ------------------------------------------------------

BrowserFontDescription::BrowserFontDescription() {
  pp_font_description_.face = face_.pp_var();
  set_family(PP_BROWSERFONT_TRUSTED_FAMILY_DEFAULT);
  set_size(0);
  set_weight(PP_BROWSERFONT_TRUSTED_WEIGHT_NORMAL);
  set_italic(false);
  set_small_caps(false);
  set_letter_spacing(0);
  set_word_spacing(0);
}

BrowserFontDescription::BrowserFontDescription(
    const BrowserFontDescription& other) {
  set_face(other.face());
  set_family(other.family());
  set_size(other.size());
  set_weight(other.weight());
  set_italic(other.italic());
  set_small_caps(other.small_caps());
  set_letter_spacing(other.letter_spacing());
  set_word_spacing(other.word_spacing());
}

BrowserFontDescription::~BrowserFontDescription() {
}

BrowserFontDescription& BrowserFontDescription::operator=(
    const BrowserFontDescription& other) {
  pp_font_description_ = other.pp_font_description_;

  // Be careful about the refcount of the string, the copy that operator= made
  // above didn't copy a ref.
  pp_font_description_.face = PP_MakeUndefined();
  set_face(other.face());

  return *this;
}

// BrowserFontTextRun ----------------------------------------------------------

BrowserFontTextRun::BrowserFontTextRun() {
  pp_text_run_.text = text_.pp_var();
  pp_text_run_.rtl = PP_FALSE;
  pp_text_run_.override_direction = PP_FALSE;
}

BrowserFontTextRun::BrowserFontTextRun(const std::string& text,
                                       bool rtl,
                                       bool override_direction)
    : text_(text) {
  pp_text_run_.text = text_.pp_var();
  pp_text_run_.rtl = PP_FromBool(rtl);
  pp_text_run_.override_direction = PP_FromBool(override_direction);
}

BrowserFontTextRun::BrowserFontTextRun(const BrowserFontTextRun& other)
    : text_(other.text_) {
  pp_text_run_.text = text_.pp_var();
  pp_text_run_.rtl = other.pp_text_run_.rtl;
  pp_text_run_.override_direction = other.pp_text_run_.override_direction;
}

BrowserFontTextRun::~BrowserFontTextRun() {
}

BrowserFontTextRun& BrowserFontTextRun::operator=(
    const BrowserFontTextRun& other) {
  pp_text_run_ = other.pp_text_run_;
  text_ = other.text_;
  pp_text_run_.text = text_.pp_var();
  return *this;
}

// BrowserFont_Trusted ---------------------------------------------------------

BrowserFont_Trusted::BrowserFont_Trusted() : Resource() {
}

BrowserFont_Trusted::BrowserFont_Trusted(PP_Resource resource)
    : Resource(resource) {
}

BrowserFont_Trusted::BrowserFont_Trusted(
    const InstanceHandle& instance,
    const BrowserFontDescription& description) {
  if (has_interface<PPB_BrowserFont_Trusted_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_BrowserFont_Trusted_1_0>()->Create(
        instance.pp_instance(),
        &description.pp_font_description()));
  }
}

BrowserFont_Trusted::BrowserFont_Trusted(const BrowserFont_Trusted& other)
    : Resource(other) {
}

BrowserFont_Trusted& BrowserFont_Trusted::operator=(
    const BrowserFont_Trusted& other) {
  Resource::operator=(other);
  return *this;
}

// static
Var BrowserFont_Trusted::GetFontFamilies(const InstanceHandle& instance) {
  if (!has_interface<PPB_BrowserFont_Trusted_1_0>())
    return Var();

  return Var(PASS_REF,
             get_interface<PPB_BrowserFont_Trusted_1_0>()->GetFontFamilies(
                 instance.pp_instance()));
}

bool BrowserFont_Trusted::Describe(
    BrowserFontDescription* description,
    PP_BrowserFont_Trusted_Metrics* metrics) const {
  // Be careful with ownership of the |face| string. It will come back with
  // a ref of 1, which we want to assign to the |face_| member of the C++ class.
  if (has_interface<PPB_BrowserFont_Trusted_1_0>()) {
    if (!get_interface<PPB_BrowserFont_Trusted_1_0>()->Describe(
        pp_resource(), &description->pp_font_description_, metrics))
      return false;
  }
  description->face_ = Var(PASS_REF,
                           description->pp_font_description_.face);
  return true;
}

bool BrowserFont_Trusted::DrawTextAt(ImageData* dest,
                                     const BrowserFontTextRun& text,
                                     const Point& position,
                                     uint32_t color,
                                     const Rect& clip,
                                     bool image_data_is_opaque) const {
  if (has_interface<PPB_BrowserFont_Trusted_1_0>()) {
    return PP_ToBool(get_interface<PPB_BrowserFont_Trusted_1_0>()->DrawTextAt(
        pp_resource(),
        dest->pp_resource(),
        &text.pp_text_run(),
        &position.pp_point(),
        color,
        &clip.pp_rect(),
        PP_FromBool(image_data_is_opaque)));
  }
  return false;
}

int32_t BrowserFont_Trusted::MeasureText(const BrowserFontTextRun& text) const {
  if (has_interface<PPB_BrowserFont_Trusted_1_0>()) {
    return get_interface<PPB_BrowserFont_Trusted_1_0>()->MeasureText(
        pp_resource(),
        &text.pp_text_run());
  }
  return -1;
}

uint32_t BrowserFont_Trusted::CharacterOffsetForPixel(
    const BrowserFontTextRun& text,
    int32_t pixel_position) const {
  if (has_interface<PPB_BrowserFont_Trusted_1_0>()) {
    return get_interface<PPB_BrowserFont_Trusted_1_0>()->
        CharacterOffsetForPixel(
            pp_resource(),
            &text.pp_text_run(),
            pixel_position);
  }
  return 0;
}

int32_t BrowserFont_Trusted::PixelOffsetForCharacter(
    const BrowserFontTextRun& text,
    uint32_t char_offset) const {
  if (has_interface<PPB_BrowserFont_Trusted_1_0>()) {
    return get_interface<PPB_BrowserFont_Trusted_1_0>()->
        PixelOffsetForCharacter(
           pp_resource(),
           &text.pp_text_run(),
           char_offset);
  }
  return 0;
}

bool BrowserFont_Trusted::DrawSimpleText(
    ImageData* dest,
    const std::string& text,
    const Point& position,
    uint32_t color,
    bool image_data_is_opaque) const {
  return DrawTextAt(dest, BrowserFontTextRun(text), position, color,
                    Rect(dest->size()), image_data_is_opaque);
}

int32_t BrowserFont_Trusted::MeasureSimpleText(const std::string& text) const {
  return MeasureText(BrowserFontTextRun(text));
}

}  // namespace pp
