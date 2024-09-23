// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_font_helpers.h"

#include <algorithm>
#include <optional>

#include "base/i18n/encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/platform/web_font_description.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/pdfium/public/fpdf_sysfontinfo.h"

namespace chrome_pdf {

namespace {

blink::WebFontDescription::Weight WeightToBlinkWeight(int weight) {
  static_assert(blink::WebFontDescription::kWeight100 == 0, "Blink Weight min");
  static_assert(blink::WebFontDescription::kWeight900 == 8, "Blink Weight max");
  constexpr int kMinimumWeight = 100;
  constexpr int kMaximumWeight = 900;
  int normalized_weight = std::clamp(weight, kMinimumWeight, kMaximumWeight);
  normalized_weight = (normalized_weight / 100) - 1;
  return static_cast<blink::WebFontDescription::Weight>(normalized_weight);
}

}  // namespace

std::optional<blink::WebFontDescription> PdfFontToBlinkFontMapping(
    int weight,
    int italic,
    int charset,
    int pitch_family,
    const char* face) {
  // Pretend the system does not have the Symbol font to force a fallback to
  // the built in Symbol font in CFX_FontMapper::FindSubstFont().
  if (strcmp(face, "Symbol") == 0) {
    return std::nullopt;
  }

  blink::WebFontDescription desc;
  if (pitch_family & FXFONT_FF_FIXEDPITCH) {
    desc.generic_family = blink::WebFontDescription::kGenericFamilyMonospace;
  } else if (pitch_family & FXFONT_FF_ROMAN) {
    desc.generic_family = blink::WebFontDescription::kGenericFamilySerif;
  } else {
    desc.generic_family = blink::WebFontDescription::kGenericFamilyStandard;
  }

  static constexpr struct {
    const char* pdf_name;
    const char* face;
    bool bold;
    bool italic;
  } kPdfFontSubstitutions[] = {
      {"Courier", "Courier New", false, false},
      {"Courier-Bold", "Courier New", true, false},
      {"Courier-BoldOblique", "Courier New", true, true},
      {"Courier-Oblique", "Courier New", false, true},
      {"Helvetica", "Arial", false, false},
      {"Helvetica-Bold", "Arial", true, false},
      {"Helvetica-BoldOblique", "Arial", true, true},
      {"Helvetica-Oblique", "Arial", false, true},
      {"Times-Roman", "Times New Roman", false, false},
      {"Times-Bold", "Times New Roman", true, false},
      {"Times-BoldItalic", "Times New Roman", true, true},
      {"Times-Italic", "Times New Roman", false, true},

      // MS P?(Mincho|Gothic) are the most notable fonts in Japanese PDF files
      // without embedding the glyphs. Sometimes the font names are encoded
      // in Japanese Windows's locale (CP932/Shift_JIS) without space.
      // Most Linux systems don't have the exact font, but for outsourcing
      // fontconfig to find substitutable font in the system, we pass ASCII
      // font names to it.
      {"MS-PGothic", "MS PGothic", false, false},
      {"MS-Gothic", "MS Gothic", false, false},
      {"MS-PMincho", "MS PMincho", false, false},
      {"MS-Mincho", "MS Mincho", false, false},
      // MS PGothic in Shift_JIS encoding.
      {"\x82\x6C\x82\x72\x82\x6F\x83\x53\x83\x56\x83\x62\x83\x4E", "MS PGothic",
       false, false},
      // MS Gothic in Shift_JIS encoding.
      {"\x82\x6C\x82\x72\x83\x53\x83\x56\x83\x62\x83\x4E", "MS Gothic", false,
       false},
      // MS PMincho in Shift_JIS encoding.
      {"\x82\x6C\x82\x72\x82\x6F\x96\xBE\x92\xA9", "MS PMincho", false, false},
      // MS Mincho in Shift_JIS encoding.
      {"\x82\x6C\x82\x72\x96\xBE\x92\xA9", "MS Mincho", false, false},
  };

  // Similar logic exists in PDFium's CFX_FolderFontInfo::FindFont().
  if (charset == FXFONT_ANSI_CHARSET && (pitch_family & FXFONT_FF_FIXEDPITCH)) {
    face = "Courier New";
  }

  // Map from the standard PDF fonts to TrueType font names.
  bool found_substitution = false;
  for (const auto& substitution : kPdfFontSubstitutions) {
    if (strcmp(face, substitution.pdf_name) == 0) {
      desc.family = blink::WebString::FromUTF8(substitution.face);
      if (substitution.bold) {
        desc.weight = blink::WebFontDescription::kWeightBold;
      }
      if (substitution.italic) {
        desc.italic = true;
      }
      found_substitution = true;
      break;
    }
  }

  if (!found_substitution) {
    // Convert to UTF-8 and make sure it is valid.
    std::string face_utf8;
    if (base::IsStringUTF8(face)) {
      face_utf8 = face;
    } else {
      std::string encoding;
      if (base::DetectEncoding(face, &encoding)) {
        // ConvertToUtf8AndNormalize() clears `face_utf8` on failure.
        base::ConvertToUtf8AndNormalize(face, encoding, &face_utf8);
      }
    }

    if (face_utf8.empty()) {
      return std::nullopt;
    }

    desc.family = blink::WebString::FromUTF8(face_utf8);
    desc.weight = WeightToBlinkWeight(weight);
    desc.italic = italic > 0;
  }

  return desc;
}

}  // namespace chrome_pdf
