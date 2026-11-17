// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/history_mediator.h"

#import <memory>
#import <string>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/history/core/common/pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/history/ui_bundled/history_consumer.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

@interface HistoryMediator () <PrefObserverDelegate>
@end

@implementation HistoryMediator {
  // Keyed service used to retrieve favicon images.
  raw_ptr<FaviconLoader> _faviconLoader;
  // Pref service used to read and observe the history deletion policy.
  raw_ptr<PrefService> _prefService;
  // Pref observer bridge and registrar for live policy updates.
  // `_prefChangeRegistrar` holds an unretained pointer to
  // `_prefObserverBridge`, so `_prefObserverBridge` must be declared first to
  // outlive it.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  PrefChangeRegistrar _prefChangeRegistrar;
}

#pragma mark - Initialization

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
                          prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    CHECK(faviconLoader);
    CHECK(prefService);
    _faviconLoader = faviconLoader;
    _prefService = prefService;
    _prefChangeRegistrar.Init(_prefService);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kAllowDeletingBrowserHistory, &_prefChangeRegistrar);
  }
  return self;
}

#pragma mark - Properties

- (void)setConsumer:(id<HistoryConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (!_consumer || !_prefService) {
    return;
  }
  [_consumer
      setHistoryDeletionAllowed:_prefService->GetBoolean(
                                    prefs::kAllowDeletingBrowserHistory)];
}

#pragma mark - Public

- (void)disconnect {
  self.consumer = nil;
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefService = nullptr;
  _faviconLoader = nullptr;
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes* attributes,
                                    bool cached))completion {
  if (!_faviconLoader) {
    return;
  }
  _faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false,
      ^(FaviconAttributes* attributes, bool cached) {
        completion(attributes, cached);
      });
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kAllowDeletingBrowserHistory) {
    [self.consumer
        setHistoryDeletionAllowed:_prefService->GetBoolean(
                                      prefs::kAllowDeletingBrowserHistory)];
  }
}

@end
