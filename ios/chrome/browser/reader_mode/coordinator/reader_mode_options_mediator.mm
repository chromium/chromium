// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_options_mediator.h"

#import "components/dom_distiller/core/distilled_page_prefs.h"
#import "components/dom_distiller/ios/distilled_page_prefs_observer_bridge.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_font_size_utils.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
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
    // Initialize consumer with current state of `_distilledPagePrefs`.
    [self.consumer setSelectedFontFamily:_distilledPagePrefs->GetFontFamily()];
    [self.consumer setSelectedTheme:_distilledPagePrefs->GetTheme()];
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
  _distilledPagePrefs->SetUserPrefTheme(theme);
}

- (void)hideReaderMode {
  [self.readerModeHandler hideReaderMode];
}

#pragma mark - Public

- (void)disconnect {
  if (_distilledPagePrefs) {
    _distilledPagePrefs->RemoveObserver(_prefsObserverBridge.get());
  }
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
  [self.consumer setDecreaseFontSizeButtonEnabled:CanDecreaseReaderModeFontSize(
                                                      _distilledPagePrefs)];
  [self.consumer setIncreaseFontSizeButtonEnabled:CanIncreaseReaderModeFontSize(
                                                      _distilledPagePrefs)];
  [self.consumer announceFontSizeMultiplier:scaling];
}

@end
