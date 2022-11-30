// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_ACTIVE_SAMPLING_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_ACTIVE_SAMPLING_H_

#include <string>
#include <vector>

#include "third_party/blink/public/common/common_export.h"

class SkFontMgr;

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace blink {

class BLINK_COMMON_EXPORT IdentifiabilityActiveSampler {
 public:
  // This class should not be instantiated. It is just a wrapper for a static
  // method, which needed to be put inside a class in order to become friend of
  // `base::ScopedAllowBaseSyncPrimitives`.
  IdentifiabilityActiveSampler() = delete;

  // Check and report availability of a set of fonts.
  static void ActivelySampleAvailableFonts(ukm::UkmRecorder* recorder);

 private:
  static bool IsFontFamilyAvailable(const char* family, SkFontMgr* fm);

  static void ReportAvailableFontFamilies(
      std::vector<std::string> fonts_to_check,
      ukm::UkmRecorder* ukm_recorder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_ACTIVE_SAMPLING_H_
