// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_mediator.h"

#import "components/web_distiller/core/distilled_page_prefs.h"
#import "components/web_distiller/ios/distilled_page_prefs_observer_bridge.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_font_size_utils.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/reader_mode/ui/reader_mode_options_consumer.h"

@interface ReaderModeOptionsMediator () <DistilledPagePrefsObserving>
@end

@implementation ReaderModeOptionsMediator {
  std::unique_ptr<DistilledPagePrefsObserverBridge> _prefsObserverBridge;
  raw_ptr<web_distiller::DistilledPagePrefs> _distilledPagePrefs;
}

- (instancetype)initWithDistilledPagePrefs:
    (web_distiller::DistilledPagePrefs*)distilledPagePrefs {
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

- (void)setFontFamily:(web_distiller::mojom::FontFamily)fontFamily {
  _distilledPagePrefs->SetFontFamily(fontFamily);
}

- (void)increaseFontSize {
  IncreaseReaderModeFontSize(_distilledPagePrefs);
}

- (void)decreaseFontSize {
  DecreaseReaderModeFontSize(_distilledPagePrefs);
}

- (void)setTheme:(web_distiller::mojom::Theme)theme {
  web_distiller::ThemeSettingsUpdateSource currentSource =
      _distilledPagePrefs->GetThemeSettingsUpdateSource();

  switch (currentSource) {
    case web_distiller::ThemeSettingsUpdateSource::kSystem: {
      _distilledPagePrefs->SetUserPrefTheme(theme);
      break;
    }
    case web_distiller::ThemeSettingsUpdateSource::kUserPreference: {
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

- (void)onChangeFontFamily:(web_distiller::mojom::FontFamily)font {
  [self.consumer setSelectedFontFamily:font];
}

- (void)onChangeTheme:(web_distiller::mojom::Theme)theme
           withSource:(web_distiller::ThemeSettingsUpdateSource)source {
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
