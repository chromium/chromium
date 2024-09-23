// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
#include <hb.h>
#include <hb-aat.h>
#include <hb-cplusplus.hh>
// clang-format on

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_caps_support.h"

namespace blink {

namespace {

bool activationSelectorPresent(
    hb_face_t* hb_face,
    const hb_aat_layout_feature_type_t feature_type,
    const hb_aat_layout_feature_selector_t enabled_selector_expectation) {
  Vector<hb_aat_layout_feature_selector_info_t> feature_selectors;
  unsigned num_feature_selectors = 0;
  unsigned default_index = 0;
  num_feature_selectors = hb_aat_layout_feature_type_get_selector_infos(
      hb_face, feature_type, 0, nullptr, nullptr, nullptr);
  feature_selectors.resize(num_feature_selectors);
  if (!hb_aat_layout_feature_type_get_selector_infos(
          hb_face, feature_type, 0, &num_feature_selectors,
          feature_selectors.data(), &default_index)) {
    return false;
  }
  for (hb_aat_layout_feature_selector_info_t selector_info :
       feature_selectors) {
    if (selector_info.enable == enabled_selector_expectation)
      return true;
  }
  return false;
}
}  // namespace

OpenTypeCapsSupport::OpenTypeCapsSupport()
    : harfbuzz_face_(nullptr),
      font_support_(FontSupport::kFull),
      caps_synthesis_(CapsSynthesis::kNone),
      font_format_(FontFormat::kUndetermined) {}

OpenTypeCapsSupport::OpenTypeCapsSupport(
    const HarfBuzzFace* harfbuzz_face,
    FontDescription::FontVariantCaps requested_caps,
    FontDescription::FontSynthesisSmallCaps font_synthesis_small_caps,
    hb_script_t script)
    : harfbuzz_face_(harfbuzz_face),
      requested_caps_(requested_caps),
      font_synthesis_small_caps_(font_synthesis_small_caps),
      font_support_(FontSupport::kFull),
      caps_synthesis_(CapsSynthesis::kNone),
      font_format_(FontFormat::kUndetermined) {
  if (requested_caps != FontDescription::kCapsNormal)
    DetermineFontSupport(script);
}

FontDescription::FontVariantCaps OpenTypeCapsSupport::FontFeatureToUse(
    SmallCapsIterator::SmallCapsBehavior source_text_case) {
  if (font_support_ == FontSupport::kFull)
    return requested_caps_;

  if (font_support_ == FontSupport::kFallback) {
    if (requested_caps_ == FontDescription::FontVariantCaps::kAllPetiteCaps)
      return FontDescription::FontVariantCaps::kAllSmallCaps;

    if (requested_caps_ == FontDescription::FontVariantCaps::kPetiteCaps ||
        (requested_caps_ == FontDescription::FontVariantCaps::kUnicase &&
         source_text_case == SmallCapsIterator::kSmallCapsSameCase))
      return FontDescription::FontVariantCaps::kSmallCaps;
  }

  return FontDescription::FontVariantCaps::kCapsNormal;
}

bool OpenTypeCapsSupport::NeedsRunCaseSplitting() {
  // Lack of titling case support is ignored, titling case is not synthesized.
  return font_support_ != FontSupport::kFull &&
         requested_caps_ != FontDescription::kTitlingCaps &&
         SyntheticSmallCapsAllowed();
}

bool OpenTypeCapsSupport::NeedsSyntheticFont(
    SmallCapsIterator::SmallCapsBehavior run_case) {
  if (font_support_ == FontSupport::kFull)
    return false;

  if (requested_caps_ == FontDescription::kTitlingCaps)
    return false;

  if (!SyntheticSmallCapsAllowed())
    return false;

  if (font_support_ == FontSupport::kNone) {
    if (run_case == SmallCapsIterator::kSmallCapsUppercaseNeeded &&
        (caps_synthesis_ == CapsSynthesis::kLowerToSmallCaps ||
         caps_synthesis_ == CapsSynthesis::kBothToSmallCaps))
      return true;

    if (run_case == SmallCapsIterator::kSmallCapsSameCase &&
        (caps_synthesis_ == CapsSynthesis::kUpperToSmallCaps ||
         caps_synthesis_ == CapsSynthesis::kBothToSmallCaps)) {
      return true;
    }
  }

  return false;
}

CaseMapIntend OpenTypeCapsSupport::NeedsCaseChange(
    SmallCapsIterator::SmallCapsBehavior run_case) {
  CaseMapIntend case_map_intend = CaseMapIntend::kKeepSameCase;

  if (font_support_ == FontSupport::kFull || !SyntheticSmallCapsAllowed())
    return case_map_intend;

  switch (run_case) {
    case SmallCapsIterator::kSmallCapsSameCase:
      case_map_intend =
          font_support_ == FontSupport::kFallback &&
                  (caps_synthesis_ == CapsSynthesis::kBothToSmallCaps ||
                   caps_synthesis_ == CapsSynthesis::kUpperToSmallCaps)
              ? CaseMapIntend::kLowerCase
              : CaseMapIntend::kKeepSameCase;
      break;
    case SmallCapsIterator::kSmallCapsUppercaseNeeded:
      case_map_intend =
          font_support_ != FontSupport::kFallback &&
                  (caps_synthesis_ == CapsSynthesis::kLowerToSmallCaps ||
                   caps_synthesis_ == CapsSynthesis::kBothToSmallCaps)
              ? CaseMapIntend::kUpperCase
              : CaseMapIntend::kKeepSameCase;
      break;
    default:
      break;
  }
  return case_map_intend;
}

OpenTypeCapsSupport::FontFormat OpenTypeCapsSupport::GetFontFormat() const {
  if (font_format_ == FontFormat::kUndetermined) {
    hb_face_t* const hb_face =
        hb_font_get_face(harfbuzz_face_->GetScaledFont());

    hb::unique_ptr<hb_blob_t> morx_blob(
        hb_face_reference_table(hb_face, HB_TAG('m', 'o', 'r', 'x')));
    hb::unique_ptr<hb_blob_t> mort_blob(
        hb_face_reference_table(hb_face, HB_TAG('m', 'o', 'r', 't')));

    // TODO(crbug.com/911149): Use hb_aat_layout_has_substitution() for
    // has_morx_or_mort and hb_ot_layout_has_substitution() for has_gsub once is
    // exposed in HarfBuzz.
    bool has_morx_or_mort = hb_blob_get_length(morx_blob.get()) ||
                            hb_blob_get_length(mort_blob.get());
    bool has_gsub = hb_ot_layout_has_substitution(hb_face);
    font_format_ = has_morx_or_mort && !has_gsub ? FontFormat::kAat
                                                 : FontFormat::kOpenType;
  }
  return font_format_;
}

bool OpenTypeCapsSupport::SupportsFeature(hb_script_t script,
                                          uint32_t tag) const {
  if (GetFontFormat() == FontFormat::kAat)
    return SupportsAatFeature(tag);
  return SupportsOpenTypeFeature(script, tag);
}

bool OpenTypeCapsSupport::SupportsAatFeature(uint32_t tag) const {
  // We only want to detect small-caps and capitals-to-small-capitals features
  // for aat-fonts, any other requests are returned as not supported.
  if (tag != HB_TAG('s', 'm', 'c', 'p') && tag != HB_TAG('c', '2', 's', 'c')) {
    return false;
  }

  hb_face_t* const hb_face = hb_font_get_face(harfbuzz_face_->GetScaledFont());

  Vector<hb_aat_layout_feature_type_t> aat_features;
  unsigned feature_count =
      hb_aat_layout_get_feature_types(hb_face, 0, nullptr, nullptr);
  aat_features.resize(feature_count);
  if (!hb_aat_layout_get_feature_types(hb_face, 0, &feature_count,
                                       aat_features.data()))
    return false;

  if (tag == HB_TAG('s', 'm', 'c', 'p')) {
    // Check for presence of new style (feature id 38) or old style (letter
    // case, feature id 3) small caps feature presence, then check for the
    // specific required activation selectors.
    if (!aat_features.Contains(HB_AAT_LAYOUT_FEATURE_TYPE_LETTER_CASE) &&
        !aat_features.Contains(HB_AAT_LAYOUT_FEATURE_TYPE_LOWER_CASE))
      return false;

    // Check for new style small caps, feature id 38.
    if (aat_features.Contains(HB_AAT_LAYOUT_FEATURE_TYPE_LOWER_CASE)) {
      if (activationSelectorPresent(
              hb_face, HB_AAT_LAYOUT_FEATURE_TYPE_LOWER_CASE,
              HB_AAT_LAYOUT_FEATURE_SELECTOR_LOWER_CASE_SMALL_CAPS))
        return true;
    }

    // Check for old style small caps enabling selector, feature id 3.
    if (aat_features.Contains(HB_AAT_LAYOUT_FEATURE_TYPE_LETTER_CASE)) {
      if (activationSelectorPresent(hb_face,
                                    HB_AAT_LAYOUT_FEATURE_TYPE_LETTER_CASE,
                                    HB_AAT_LAYOUT_FEATURE_SELECTOR_SMALL_CAPS))
        return true;
    }

    // Neither old or new style small caps present.
    return false;
  }

  if (tag == HB_TAG('c', '2', 's', 'c')) {
    if (!aat_features.Contains(HB_AAT_LAYOUT_FEATURE_TYPE_UPPER_CASE))
      return false;

    return activationSelectorPresent(
        hb_face, HB_AAT_LAYOUT_FEATURE_TYPE_UPPER_CASE,
        HB_AAT_LAYOUT_FEATURE_SELECTOR_UPPER_CASE_SMALL_CAPS);
  }

  return false;
}

void OpenTypeCapsSupport::DetermineFontSupport(hb_script_t script) {
  switch (requested_caps_) {
    case FontDescription::kSmallCaps:
      if (!SupportsFeature(script, HB_TAG('s', 'm', 'c', 'p'))) {
        font_support_ = FontSupport::kNone;
        caps_synthesis_ = CapsSynthesis::kLowerToSmallCaps;
      }
      break;
    case FontDescription::kAllSmallCaps:
      if (!(SupportsFeature(script, HB_TAG('s', 'm', 'c', 'p')) &&
            SupportsFeature(script, HB_TAG('c', '2', 's', 'c')))) {
        font_support_ = FontSupport::kNone;
        caps_synthesis_ = CapsSynthesis::kBothToSmallCaps;
      }
      break;
    case FontDescription::kPetiteCaps:
      if (!SupportsFeature(script, HB_TAG('p', 'c', 'a', 'p'))) {
        if (SupportsFeature(script, HB_TAG('s', 'm', 'c', 'p'))) {
          font_support_ = FontSupport::kFallback;
        } else {
          font_support_ = FontSupport::kNone;
          caps_synthesis_ = CapsSynthesis::kLowerToSmallCaps;
        }
      }
      break;
    case FontDescription::kAllPetiteCaps:
      if (!(SupportsFeature(script, HB_TAG('p', 'c', 'a', 'p')) &&
            SupportsFeature(script, HB_TAG('c', '2', 'p', 'c')))) {
        if (SupportsFeature(script, HB_TAG('s', 'm', 'c', 'p')) &&
            SupportsFeature(script, HB_TAG('c', '2', 's', 'c'))) {
          font_support_ = FontSupport::kFallback;
        } else {
          font_support_ = FontSupport::kNone;
          caps_synthesis_ = CapsSynthesis::kBothToSmallCaps;
        }
      }
      break;
    case FontDescription::kUnicase:
      if (!SupportsFeature(script, HB_TAG('u', 'n', 'i', 'c'))) {
        caps_synthesis_ = CapsSynthesis::kUpperToSmallCaps;
        if (SupportsFeature(script, HB_TAG('s', 'm', 'c', 'p'))) {
          font_support_ = FontSupport::kFallback;
        } else {
          font_support_ = FontSupport::kNone;
        }
      }
      break;
    case FontDescription::kTitlingCaps:
      if (!SupportsFeature(script, HB_TAG('t', 'i', 't', 'l'))) {
        font_support_ = FontSupport::kNone;
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

bool OpenTypeCapsSupport::SyntheticSmallCapsAllowed() const {
  return font_synthesis_small_caps_ ==
         FontDescription::kAutoFontSynthesisSmallCaps;
}

}  // namespace blink
