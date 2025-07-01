// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_TRUSTED_BROWSER_FONT_TRUSTED_H_
#define PPAPI_CPP_TRUSTED_BROWSER_FONT_TRUSTED_H_

#include <stdint.h>

#include <string>

#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

namespace pp {

class ImageData;
class InstanceHandle;
class Point;
class Rect;

// BrowserFontDescription ------------------------------------------------------

class BrowserFontDescription {
 public:
  BrowserFontDescription();
  BrowserFontDescription(const BrowserFontDescription& other);
  ~BrowserFontDescription();

  BrowserFontDescription& operator=(const BrowserFontDescription& other);

  const PP_BrowserFont_Trusted_Description& pp_font_description() const {
    return pp_font_description_;
  }

  Var face() const { return face_; }
  void set_face(const Var& face) {
    face_ = face;
    pp_font_description_.face = face_.pp_var();
  }

  PP_BrowserFont_Trusted_Family family() const {
    return pp_font_description_.family;
  }
  void set_family(PP_BrowserFont_Trusted_Family f) {
    pp_font_description_.family = f;
  }

  uint32_t size() const { return pp_font_description_.size; }
  void set_size(uint32_t s) { pp_font_description_.size = s; }

  PP_BrowserFont_Trusted_Weight weight() const {
    return pp_font_description_.weight;
  }
  void set_weight(PP_BrowserFont_Trusted_Weight w) {
    pp_font_description_.weight = w;
  }

  bool italic() const { return PP_ToBool(pp_font_description_.italic); }
  void set_italic(bool i) { pp_font_description_.italic = PP_FromBool(i); }

  bool small_caps() const {
    return PP_ToBool(pp_font_description_.small_caps);
  }
  void set_small_caps(bool s) {
    pp_font_description_.small_caps = PP_FromBool(s);
  }

  int letter_spacing() const { return pp_font_description_.letter_spacing; }
  void set_letter_spacing(int s) { pp_font_description_.letter_spacing = s; }

  int word_spacing() const { return pp_font_description_.word_spacing; }
  void set_word_spacing(int w) { pp_font_description_.word_spacing = w; }

 private:
  friend class BrowserFont_Trusted;

  Var face_;  // Manages memory for pp_font_description_.face
  PP_BrowserFont_Trusted_Description pp_font_description_;
};

// BrowserFontTextRun ----------------------------------------------------------

class BrowserFontTextRun {
 public:
  BrowserFontTextRun();
  BrowserFontTextRun(const std::string& text,
                     bool rtl = false,
                     bool override_direction = false);
  BrowserFontTextRun(const BrowserFontTextRun& other);
  ~BrowserFontTextRun();

  BrowserFontTextRun& operator=(const BrowserFontTextRun& other);

  const PP_BrowserFont_Trusted_TextRun& pp_text_run() const {
    return pp_text_run_;
  }

 private:
  Var text_;  // Manages memory for the reference in pp_text_run_.
  PP_BrowserFont_Trusted_TextRun pp_text_run_;
};

// BrowserFont_Trusted ---------------------------------------------------------

// Provides access to system fonts.
class BrowserFont_Trusted : public Resource {
 public:
  // Creates an is_null() Font object.
  BrowserFont_Trusted();

  explicit BrowserFont_Trusted(PP_Resource resource);
  BrowserFont_Trusted(const InstanceHandle& instance,
                      const BrowserFontDescription& description);
  BrowserFont_Trusted(const BrowserFont_Trusted& other);

  BrowserFont_Trusted& operator=(const BrowserFont_Trusted& other);

  // PPB_Font methods:
  static Var GetFontFamilies(const InstanceHandle& instance);
  bool Describe(BrowserFontDescription* description,
                PP_BrowserFont_Trusted_Metrics* metrics) const;
  bool DrawTextAt(ImageData* dest,
                  const BrowserFontTextRun& text,
                  const Point& position,
                  uint32_t color,
                  const Rect& clip,
                  bool image_data_is_opaque) const;
  int32_t MeasureText(const BrowserFontTextRun& text) const;
  uint32_t CharacterOffsetForPixel(const BrowserFontTextRun& text,
                                   int32_t pixel_position) const;
  int32_t PixelOffsetForCharacter(const BrowserFontTextRun& text,
                                  uint32_t char_offset) const;

  // Convenience function that assumes a left-to-right string with no clipping.
  bool DrawSimpleText(ImageData* dest,
                      const std::string& text,
                      const Point& position,
                      uint32_t color,
                      bool image_data_is_opaque = false) const;

  // Convenience function that assumes a left-to-right string.
  int32_t MeasureSimpleText(const std::string& text) const;
};

}  // namespace pp

#endif  // PPAPI_CPP_TRUSTED_BROWSER_FONT_TRUSTED_H_
