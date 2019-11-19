// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_font_linux.h"

#include <algorithm>
#include <string>

#include "base/i18n/encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/pdf.h"
#include "ppapi/cpp/trusted/browser_font_trusted.h"
#include "third_party/pdfium/public/fpdf_sysfontinfo.h"

namespace chrome_pdf {

namespace {

PP_Instance g_last_instance_id;

PP_BrowserFont_Trusted_Weight WeightToBrowserFontTrustedWeight(int weight) {
  static_assert(PP_BROWSERFONT_TRUSTED_WEIGHT_100 == 0,
                "PP_BrowserFont_Trusted_Weight min");
  static_assert(PP_BROWSERFONT_TRUSTED_WEIGHT_900 == 8,
                "PP_BrowserFont_Trusted_Weight max");
  constexpr int kMinimumWeight = 100;
  constexpr int kMaximumWeight = 900;
  int normalized_weight =
      base::ClampToRange(weight, kMinimumWeight, kMaximumWeight);
  normalized_weight = (normalized_weight / 100) - 1;
  return static_cast<PP_BrowserFont_Trusted_Weight>(normalized_weight);
}

// This list is for CPWL_FontMap::GetDefaultFontByCharset().
// We pretend to have these font natively and let the browser (or underlying
// fontconfig) pick the proper font on the system.
void EnumFonts(FPDF_SYSFONTINFO* sysfontinfo, void* mapper) {
  FPDF_AddInstalledFont(mapper, "Arial", FXFONT_DEFAULT_CHARSET);

  const FPDF_CharsetFontMap* font_map = FPDF_GetDefaultTTFMap();
  for (; font_map->charset != -1; ++font_map) {
    FPDF_AddInstalledFont(mapper, font_map->fontname, font_map->charset);
  }
}

void* MapFont(FPDF_SYSFONTINFO*,
              int weight,
              int italic,
              int charset,
              int pitch_family,
              const char* face,
              int* exact) {
  // Do not attempt to map fonts if PPAPI is not initialized (for Privet local
  // printing).
  // TODO(noamsml): Real font substitution (http://crbug.com/391978)
  if (!pp::Module::Get())
    return nullptr;

  pp::BrowserFontDescription description;

  // Pretend the system does not have the Symbol font to force a fallback to
  // the built in Symbol font in CFX_FontMapper::FindSubstFont().
  if (strcmp(face, "Symbol") == 0)
    return nullptr;

  if (pitch_family & FXFONT_FF_FIXEDPITCH) {
    description.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE);
  } else if (pitch_family & FXFONT_FF_ROMAN) {
    description.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_SERIF);
  }

  static const struct {
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
  if (charset == FXFONT_ANSI_CHARSET && (pitch_family & FXFONT_FF_FIXEDPITCH))
    face = "Courier New";

  // Map from the standard PDF fonts to TrueType font names.
  size_t i;
  for (i = 0; i < base::size(kPdfFontSubstitutions); ++i) {
    if (strcmp(face, kPdfFontSubstitutions[i].pdf_name) == 0) {
      description.set_face(kPdfFontSubstitutions[i].face);
      if (kPdfFontSubstitutions[i].bold)
        description.set_weight(PP_BROWSERFONT_TRUSTED_WEIGHT_BOLD);
      if (kPdfFontSubstitutions[i].italic)
        description.set_italic(true);
      break;
    }
  }

  if (i == base::size(kPdfFontSubstitutions)) {
    // Convert to UTF-8 before calling set_face().
    std::string face_utf8;
    if (base::IsStringUTF8(face)) {
      face_utf8 = face;
    } else {
      std::string encoding;
      if (base::DetectEncoding(face, &encoding)) {
        // ConvertToUtf8AndNormalize() clears |face_utf8| on failure.
        base::ConvertToUtf8AndNormalize(face, encoding, &face_utf8);
      }
    }

    if (face_utf8.empty())
      return nullptr;

    description.set_face(face_utf8);
    description.set_weight(WeightToBrowserFontTrustedWeight(weight));
    description.set_italic(italic > 0);
  }

  if (!pp::PDF::IsAvailable()) {
    NOTREACHED();
    return nullptr;
  }

  PP_Resource font_resource = pp::PDF::GetFontFileWithFallback(
      pp::InstanceHandle(g_last_instance_id),
      &description.pp_font_description(),
      static_cast<PP_PrivateFontCharset>(charset));
  long res_id = font_resource;
  return reinterpret_cast<void*>(res_id);
}

unsigned long GetFontData(FPDF_SYSFONTINFO*,
                          void* font_id,
                          unsigned int table,
                          unsigned char* buffer,
                          unsigned long buf_size) {
  if (!pp::PDF::IsAvailable()) {
    NOTREACHED();
    return 0;
  }

  uint32_t size = buf_size;
  long res_id = reinterpret_cast<long>(font_id);
  if (!pp::PDF::GetFontTableForPrivateFontFile(res_id, table, buffer, &size))
    return 0;
  return size;
}

void DeleteFont(FPDF_SYSFONTINFO*, void* font_id) {
  long res_id = reinterpret_cast<long>(font_id);
  pp::Module::Get()->core()->ReleaseResource(res_id);
}

FPDF_SYSFONTINFO g_font_info = {1,           0, EnumFonts, MapFont,   0,
                                GetFontData, 0, 0,         DeleteFont};

}  // namespace

void InitializeLinuxFontMapper() {
  FPDF_SetSystemFontInfo(&g_font_info);
}

void SetLastInstance(pp::Instance* last_instance) {
  if (last_instance)
    g_last_instance_id = last_instance->pp_instance();
}

}  // namespace chrome_pdf
