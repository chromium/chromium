// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_mediator.h"

#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/dom_distiller/ios/distilled_page_prefs_observer_bridge.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@interface ReaderModeOptionsMediator () <DistilledPagePrefsObserving>
@end

@implementation ReaderModeOptionsMediator {
  std::unique_ptr<DistilledPagePrefsObserverBridge> _prefsObserverBridge;
  raw_ptr<dom_distiller::DistilledPagePrefs> _distilledPagePrefs;
  raw_ptr<WebStateList> _webStateList;
}

- (instancetype)initWithDistilledPagePrefs:
                    (dom_distiller::DistilledPagePrefs*)distilledPagePrefs
                              webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _distilledPagePrefs = distilledPagePrefs;
    _webStateList = webStateList;
    _prefsObserverBridge =
        std::make_unique<DistilledPagePrefsObserverBridge>(self);
    _distilledPagePrefs->AddObserver(_prefsObserverBridge.get());
  }
  return self;
}

- (void)setConsumer:(id<ReaderModeOptionsConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self onChangeFontFamily:_distilledPagePrefs->GetFontFamily()];
    [self onChangeTheme:_distilledPagePrefs->GetTheme()];
    [self onChangeFontScaling:_distilledPagePrefs->GetFontScaling()];
  }
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
  _distilledPagePrefs->SetUserPrefTheme(theme);
}

- (void)hideReaderMode {
  [self.readerModeHandler hideReaderMode];
}

#pragma mark - Public

- (void)disconnect {
  _distilledPagePrefs->RemoveObserver(_prefsObserverBridge.get());
  _prefsObserverBridge.reset();
  _distilledPagePrefs = nullptr;
  _webStateList = nullptr;
}

#pragma mark - DistilledPagePrefsObserving

- (void)onChangeFontFamily:(dom_distiller::mojom::FontFamily)font {
  [self.consumer setSelectedFontFamily:font];
}

- (void)onChangeTheme:(dom_distiller::mojom::Theme)theme {
  [self.consumer setSelectedTheme:theme];
}

- (void)onChangeFontScaling:(float)scaling {
  std::vector<double> multipliers = ReaderModeFontScaleMultipliers();
  [self.consumer
      setDecreaseFontSizeButtonEnabled:(scaling > multipliers.front())];
  [self.consumer
      setIncreaseFontSizeButtonEnabled:(scaling < multipliers.back())];
}

@end
