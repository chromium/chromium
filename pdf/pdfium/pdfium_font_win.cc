// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_font_win.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_font_helpers.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/public/platform/web_font_description.h"
#include "third_party/pdfium/public/fpdf_sysfontinfo.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace chrome_pdf {

namespace {

constexpr auto kBase14Substs =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        // PDF Fonts
        {"Courier", "Courier New"},
        {"Courier-Bold", "Courier New Bold"},
        {"Courier-BoldOblique", "Courier New Bold Italic"},
        {"Courier-Oblique", "Courier New Italic"},
        {"Helvetica", "Arial"},
        {"Helvetica-Bold", "Arial Bold"},
        {"Helvetica-BoldOblique", "Arial Bold Italic"},
        {"Helvetica-Oblique", "Arial Italic"},
        {"Times-Roman", "Times New Roman"},
        {"Times-Bold", "Times New Roman Bold"},
        {"Times-BoldItalic", "Times New Roman Bold Italic"},
        {"Times-Italic", "Times New Roman Italic"},
    });

// kBase14Substs from cfx_folderfontinfo.
std::string GetSubstFont(const std::string& face) {
  auto iter = kBase14Substs.find(face);
  if (iter != kBase14Substs.end()) {
    return std::string(iter->second);
  }
  return face;
}

// Maps font description and charset to `FontId` as requested by PDFium, with
// `FontId` as an opaque type that PDFium works with. Based on the `FontId`,
// PDFium can read from the font files using GetFontData(). Properly frees the
// underlying resource type when PDFium is done with the mapped font.
class SkiaFontMapper {
 public:
  // Defined as the type most convenient for use with PDFium's
  // `FPDF_SYSFONTINFO` functions.
  using FontId = void*;

  SkiaFontMapper() { manager_ = skia::DefaultFontMgr(); }

  ~SkiaFontMapper() = delete;

  // Returns a handle to the font mapped based on `desc`, for use
  // as the `font_id` in GetFontData() and DeleteFont() below. Returns nullptr
  // on failure.
  FontId MapFont(int weight,
                 int italic,
                 int charset,
                 int pitch,
                 const char* face) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    TRACE_EVENT2("fonts", "PdfiumMapFont", "face", std::string(face), "charset",
                 charset);

    auto typeface = MapTypeface(weight, italic, charset, pitch, face);
    if (typeface) {
      FontId id = reinterpret_cast<FontId>(typeface->uniqueID());
      id_to_typeface_.try_emplace(id, std::move(typeface));
      return id;
    }

    LOG(WARNING) << "Failed to lookup face `" << base::HexEncode(face)
                 << "` for charset " << charset << ", weight " << weight;
    return nullptr;
  }

  // Releases the font file that `font_id` points to. Note that skia's font
  // manager might retain its own cached resources.
  void DeleteFont(FontId font_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    id_to_typeface_.erase(font_id);
  }

  // Reads data from the `font_id` handle for `table` into a `buffer` of
  // `buf_size`. Returns the amount of data read on success, or 0 on failure.
  // If `buffer` is null, then just return the required size for the buffer.
  // See content::GetFontTable() for information on the `table_tag` parameter.
  unsigned long GetFontData(FontId font_id,
                            unsigned int table_tag,
                            unsigned char* buffer,
                            unsigned long buf_size) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // This class creates font_id so it will always cast safely to SkTypefaceID.
    auto stored_typeface = id_to_typeface_.find(font_id);
    if (stored_typeface == id_to_typeface_.end()) {
      return 0;
    }

    sk_sp<SkTypeface> typeface = stored_typeface->second;

    // PDFium asks for 0 and 'ttcf' tags. These are not supported by DirectWrite
    // backed Skia fonts so this adapter must do something sensible. Return the
    // full font data for 0, and skip 'ttcf' allowing getTableSize(ttcf) to
    // naturally fail.
    if (table_tag == 0) {
      std::unique_ptr<SkStreamAsset> stream = typeface->openStream(nullptr);
      if (!buffer || buf_size < stream->getLength()) {
        return stream->getLength();
      }
      return stream->read(buffer, buf_size);
    }

    if (!buffer) {
      return typeface->getTableSize(table_tag);
    }

    return typeface->getTableData(table_tag, /*offset=*/0,
                                  /*size=*/buf_size, buffer);
  }

 private:
  // Lookup a typeface, taking various fallbacks if fonts are not available.
  sk_sp<SkTypeface> MapTypeface(int weight,
                                int italic,
                                int charset,
                                int pitch,
                                const std::string& face) {
    // Lookup via skia manager directly.
    SkFontStyle style(weight, SkFontStyle::Width::kNormal_Width,
                      italic > 0 ? SkFontStyle::Slant::kItalic_Slant
                                 : SkFontStyle::Slant::kUpright_Slant);

    // Force name substitution for default PDF fonts.
    std::string subst_face = GetSubstFont(face);

    auto typeface = manager_->matchFamilyStyle(subst_face.c_str(), style);
    if (typeface) {
      return typeface;
    }

    // Try pdf->blink mappings, which does its own substitution.
    std::optional<blink::WebFontDescription> desc =
        PdfFontToBlinkFontMapping(weight, italic, charset, pitch, face.c_str());
    if (desc) {
      typeface = manager_->matchFamilyStyle(desc->family.Utf8().c_str(), style);
      if (typeface) {
        return typeface;
      }
    }

    // Nothing was found (e.g. an optional Windows font is not installed),
    // then try to map the name to a fallback.
    auto fallback = GetFallbackFace(subst_face, charset, weight, italic);
    if (fallback) {
      typeface = manager_->matchFamilyStyle(fallback->c_str(), style);
      if (typeface) {
        return typeface;
      }
    }

    // Finally, try some hacks that fix edge cases & mis-spellings.
    return FinalFixups(subst_face, style);
  }

  bool HasFamily(const char* family) {
    auto style_set = manager_->matchFamily(family);
    bool has_family = style_set->count() > 0;
    return has_family;
  }

  std::optional<std::string> GetShiftJISPreference(const std::string& face,
                                                   int weight,
                                                   int pitch_family) {
    if (base::Contains(face, "Gothic") ||
        base::Contains(face, "\x83\x53\x83\x56\x83\x62\x83\x4e")) {
      if (base::Contains(face, "UI Gothic")) {
        return "MS UI Gothic";
      } else if (base::Contains(face, "PGothic") ||
                 base::Contains(face,
                                "\x82\x6f\x83\x53\x83\x56\x83\x62\x83\x4e") ||
                 base::Contains(face, "HGSGothicM") ||
                 base::Contains(face, "HGMaruGothicMPRO")) {
        return "MS PGothic";
      }
      return "MS Gothic";
    }
    if (base::Contains(face, "Mincho") ||
        base::Contains(face, "\x96\xbe\x92\xa9")) {
      if (base::Contains(face, "PMincho") ||
          base::Contains(face, "\x82\x6f\x96\xbe\x92\xa9")) {
        return std::string(HasFamily("MS PMincho") ? "MS PMincho"
                                                   : "MS PGothic");
      }
      return std::string(HasFamily("MS Mincho") ? "MS Mincho" : "MS Gothic");
    }
    if (!(pitch_family & FXFONT_FF_ROMAN) && weight > 400) {
      return "MS PGothic";
    }
    return "MS Gothic";
  }

  std::optional<std::string> GetGBPreference(const std::string& face,
                                             int weight,
                                             int pitch_family) {
    // KaiTi and SimHei are Windows supplemental fonts so assume they were not
    // found by skia.
    if (base::Contains(face, "KaiTi") || base::Contains(face, "\xbf\xac")) {
      return "SimSun";
    } else if (base::Contains(face, "FangSong") ||
               base::Contains(face, "\xb7\xc2\xcb\xce")) {
      return "SimSun";
    } else if (base::Contains(face, "SimSun") ||
               base::Contains(face, "\xcb\xce")) {
      return "SimSun";
    } else if (base::Contains(face, "SimHei") ||
               base::Contains(face, "\xba\xda")) {
      return "SimHei";
    } else if (!(pitch_family & FXFONT_FF_ROMAN) && weight > 550) {
      return "SimHei";
    }
    return "SimSun";
  }

  std::optional<std::string> GetHangeulPreference(const std::string& face,
                                                  int weight,
                                                  int pitch_family) {
    // Gulim is a supplemental font.
    if (HasFamily("Gulim")) {
      return "Gulim";
    }
    return "Malgun Gothic";
  }

  std::optional<std::string> GetFallbackFace(const std::string& face,
                                             int charset,
                                             int weight,
                                             int pitch_family) {
    switch (charset) {
      case FXFONT_SHIFTJIS_CHARSET:
        return GetShiftJISPreference(face, weight, pitch_family);
      case FXFONT_GB2312_CHARSET:
        return GetGBPreference(face, weight, pitch_family);
      case FXFONT_HANGEUL_CHARSET:
        return GetHangeulPreference(face, weight, pitch_family);
      case FXFONT_CHINESEBIG5_CHARSET:
        if (base::Contains(face, "MSung")) {
          // Monospace.
          return "Microsoft YaHei";
        }
        // Proportional.
        return "Microsoft JHengHei";
      default:
        return std::nullopt;
    }
  }

  // Put any last-gasp hacks into this method.
  sk_sp<SkTypeface> FinalFixups(const std::string& face,
                                const SkFontStyle& style) {
    // Some fonts are specified with weights that Skia can't provide.
    // pdf.js/tests/issue5801.pdf specifies ArialBlack but a weight of 390.
    // Commonly seen patterns: `ArialBlack` `Arial Black` & `Arial-Black`.
    if (base::StartsWith(face, "Arial")) {
      if (base::EndsWith(face, "Black")) {
        SkFontStyle black = SkFontStyle(SkFontStyle::Weight::kBlack_Weight,
                                        style.width(), style.slant());
        return manager_->matchFamilyStyle("Arial", black);
      }
      if (base::EndsWith(face, "Narrow")) {
        SkFontStyle narrow = SkFontStyle(SkFontStyle::Weight::kThin_Weight,
                                         style.width(), style.slant());
        return manager_->matchFamilyStyle("Arial", narrow);
      }
    }
    // Some fonts are specified without spaces in their name e.g. `ComicSansMS`.
    std::string with_spaces(face);
    // s/{lower case letter}{uppercase letter}/l u/g.
    if (re2::RE2::GlobalReplace(&with_spaces, "(\\p{Ll})(\\p{Lu})", "\\1 \\2") >
        0) {
      return manager_->matchFamilyStyle(with_spaces.c_str(), style);
    }
    return nullptr;
  }

  sk_sp<SkFontMgr> manager_;
  base::flat_map<FontId, sk_sp<SkTypeface>> id_to_typeface_;
  SEQUENCE_CHECKER(sequence_checker_);
};

SkiaFontMapper& GetSkiaFontMapper() {
  static base::NoDestructor<SkiaFontMapper> mapper;
  return *mapper;
}

// Note: `exact` is obsolete.
void* MapFont(FPDF_SYSFONTINFO*,
              int weight,
              int italic,
              int charset,
              int pitch,
              const char* face,
              int* exact) {
  // Exit early if pdfium was specifically configured in kNoMapping mode.
  if (PDFiumEngine::GetFontMappingMode() != FontMappingMode::kBlink) {
    CHECK_EQ(PDFiumEngine::GetFontMappingMode(), FontMappingMode::kNoMapping);
    return nullptr;
  }

  return GetSkiaFontMapper().MapFont(weight, italic, charset, pitch, face);
}

unsigned long GetFontData(FPDF_SYSFONTINFO*,
                          void* font_id,
                          unsigned int table,
                          unsigned char* buffer,
                          unsigned long buf_size) {
  CHECK_EQ(PDFiumEngine::GetFontMappingMode(), FontMappingMode::kBlink);
  return GetSkiaFontMapper().GetFontData(font_id, table, buffer, buf_size);
}

void DeleteFont(FPDF_SYSFONTINFO*, void* font_id) {
  CHECK_EQ(PDFiumEngine::GetFontMappingMode(), FontMappingMode::kBlink);
  GetSkiaFontMapper().DeleteFont(font_id);
}

FPDF_SYSFONTINFO g_font_info = {1,           0, 0, MapFont,   0,
                                GetFontData, 0, 0, DeleteFont};

}  // namespace

void InitializeWindowsFontMapper() {
  FPDF_SetSystemFontInfo(&g_font_info);
}

FPDF_SYSFONTINFO* GetSkiaFontMapperForTesting() {
  return &g_font_info;
}

}  // namespace chrome_pdf
