// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_mediator.h"

#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"

@implementation ReaderModeOptionsMediator {
  // The distilled page preferences.
  raw_ptr<dom_distiller::DistilledPagePrefs> _distilledPagePrefs;
}

- (instancetype)initWithDistilledPagePrefs:
    (dom_distiller::DistilledPagePrefs*)distilledPagePrefs {
  self = [super init];
  if (self) {
    _distilledPagePrefs = distilledPagePrefs;
  }
  return self;
}

#pragma mark - ReaderModeOptionsMutator

- (void)setFontFamily:(dom_distiller::mojom::FontFamily)fontFamily {
  _distilledPagePrefs->SetFontFamily(fontFamily);
}

- (void)increaseFontSize {
  double currentScaling = _distilledPagePrefs->GetFontScaling();
  std::vector<double> multipliers = ReaderModeFontScaleMultipliers();
  auto it =
      std::upper_bound(multipliers.begin(), multipliers.end(), currentScaling);
  if (it != multipliers.end()) {
    _distilledPagePrefs->SetFontScaling(*it);
  }
}

- (void)decreaseFontSize {
  double currentScaling = _distilledPagePrefs->GetFontScaling();
  std::vector<double> multipliers = ReaderModeFontScaleMultipliers();
  auto it =
      std::lower_bound(multipliers.begin(), multipliers.end(), currentScaling);
  if (it != multipliers.begin()) {
    _distilledPagePrefs->SetFontScaling(*(--it));
  }
}

- (void)setTheme:(dom_distiller::mojom::Theme)theme {
  _distilledPagePrefs->SetTheme(theme);
}

#pragma mark - Public

- (void)disconnect {
  _distilledPagePrefs = nullptr;
}

@end
