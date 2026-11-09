// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_font_size_utils.h"

#import <algorithm>
#import <vector>

#import "components/web_distiller/core/distilled_page_prefs.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"

void IncreaseReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs) {
  double current_scale = prefs->GetFontScaling();
  std::vector<double> multipliers = ReaderModeFontScaleMultipliers();
  auto it =
      std::upper_bound(multipliers.begin(), multipliers.end(), current_scale);
  if (it != multipliers.end()) {
    prefs->SetUserPrefFontScaling(*it);
  }
}

void DecreaseReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs) {
  double current_scale = prefs->GetFontScaling();
  std::vector<double> multipliers = ReaderModeFontScaleMultipliers();
  auto it =
      std::lower_bound(multipliers.begin(), multipliers.end(), current_scale);
  if (it != multipliers.begin()) {
    prefs->SetUserPrefFontScaling(*(--it));
  }
}

void ResetReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs) {
  prefs->SetUserPrefFontScaling(1.0);
}

bool CanIncreaseReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs) {
  double current_scale = prefs->GetFontScaling();
  std::vector<double> multipliers = ReaderModeFontScaleMultipliers();
  return current_scale < multipliers.back();
}

bool CanDecreaseReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs) {
  double current_scale = prefs->GetFontScaling();
  std::vector<double> multipliers = ReaderModeFontScaleMultipliers();
  return current_scale > multipliers.front();
}

bool CanResetReaderModeFontSize(web_distiller::DistilledPagePrefs* prefs) {
  return prefs->GetFontScaling() != 1.0;
}
