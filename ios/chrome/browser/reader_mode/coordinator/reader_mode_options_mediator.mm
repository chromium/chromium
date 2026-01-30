// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_mediator.h"

#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/dom_distiller/ios/distilled_page_prefs_observer_bridge.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_font_size_utils.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_consumer.h"

@interface ReaderModeOptionsMediator () <DistilledPagePrefsObserving>
@end

@implementation ReaderModeOptionsMediator {
  std::unique_ptr<DistilledPagePrefsObserverBridge> _prefsObserverBridge;
  raw_ptr<dom_distiller::DistilledPagePrefs> _distilledPagePrefs;
}

- (instancetype)initWithDistilledPagePrefs:
    (dom_distiller::DistilledPagePrefs*)distilledPagePrefs {
  self = [super init];
  if (self) {
    _distilledPagePrefs = distilledPagePrefs;
    _prefsObserverBridge = std::make_unique<DistilledPagePrefsObserverBridge>(
        self, _distilledPagePrefs);
  }
  return self;
}

- (void)setConsumer:(id<ReaderModeOptionsConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    // Initialize consumer with current state of `_distilledPagePrefs`.
    [self.consumer setSelectedFontFamily:_distilledPagePrefs->GetFontFamily()];
    [self.consumer
        setSelectedTheme:_distilledPagePrefs->GetTheme()
              fromSource:_distilledPagePrefs->GetThemeSettingsUpdateSource()];
    [self.consumer
        setDecreaseFontSizeButtonEnabled:CanDecreaseReaderModeFontSize(
                                             _distilledPagePrefs)];
    [self.consumer
        setIncreaseFontSizeButtonEnabled:CanIncreaseReaderModeFontSize(
                                             _distilledPagePrefs)];
  }
}

#pragma mark - ReaderModeOptionsMutator

- (void)setFontFamily:(dom_distiller::mojom::FontFamily)fontFamily {
  _distilledPagePrefs->SetFontFamily(fontFamily);
}

- (void)increaseFontSize {
  IncreaseReaderModeFontSize(_distilledPagePrefs);
}

- (void)decreaseFontSize {
  DecreaseReaderModeFontSize(_distilledPagePrefs);
}

- (void)setTheme:(dom_distiller::mojom::Theme)theme {
  dom_distiller::ThemeSettingsUpdateSource currentSource =
      _distilledPagePrefs->GetThemeSettingsUpdateSource();

  switch (currentSource) {
    case dom_distiller::ThemeSettingsUpdateSource::kSystem: {
      _distilledPagePrefs->SetUserPrefTheme(theme);
      break;
    }
    case dom_distiller::ThemeSettingsUpdateSource::kUserPreference: {
      if (_distilledPagePrefs->GetTheme() == theme) {
        _distilledPagePrefs->ClearUserPrefTheme();
      } else {
        _distilledPagePrefs->SetUserPrefTheme(theme);
      }
      break;
    }
  }
}

- (void)hideReaderMode {
  [self.readerModeHandler hideReaderMode];
}

#pragma mark - Public

- (void)disconnect {
  _prefsObserverBridge.reset();
  _distilledPagePrefs = nullptr;
}

#pragma mark - DistilledPagePrefsObserving

- (void)onChangeFontFamily:(dom_distiller::mojom::FontFamily)font {
  [self.consumer setSelectedFontFamily:font];
}

- (void)onChangeTheme:(dom_distiller::mojom::Theme)theme
           withSource:(dom_distiller::ThemeSettingsUpdateSource)source {
  [self.consumer setSelectedTheme:theme fromSource:source];
}

- (void)onChangeFontScaling:(float)scaling {
  [self.consumer setDecreaseFontSizeButtonEnabled:CanDecreaseReaderModeFontSize(
                                                      _distilledPagePrefs)];
  [self.consumer setIncreaseFontSizeButtonEnabled:CanIncreaseReaderModeFontSize(
                                                      _distilledPagePrefs)];
  [self.consumer announceFontSizeMultiplier:scaling];
}

@end
