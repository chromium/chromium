// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/banner_promo/model/fake_default_browser_banner_promo_app_agent.h"

@implementation FakeDefaultBrowserBannerPromoAppAgent {
  NSHashTable<id<DefaultBrowserBannerAppAgentObserver>>* _observers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _observers = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (void)addObserver:(id<DefaultBrowserBannerAppAgentObserver>)observer {
  [super addObserver:observer];
  [_observers addObject:observer];
}

- (void)removeObserver:(id<DefaultBrowserBannerAppAgentObserver>)observer {
  [super removeObserver:observer];
  [_observers removeObject:observer];
}

- (void)forceDisplayPromo {
  for (id<DefaultBrowserBannerAppAgentObserver> observer in _observers) {
    [observer displayPromoFromAppAgent:self];
  }
}

- (void)forceHidePromo {
  for (id<DefaultBrowserBannerAppAgentObserver> observer in _observers) {
    [observer hidePromoFromAppAgent:self];
  }
}

@end
