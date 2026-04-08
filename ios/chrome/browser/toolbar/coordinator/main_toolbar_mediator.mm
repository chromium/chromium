// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/main_toolbar_mediator.h"

#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@interface MainToolbarMediator () <BooleanObserver>
@end

@implementation MainToolbarMediator {
  PrefBackedBoolean* _bottomOmniboxPref;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    CHECK(prefService);
    _bottomOmniboxPref = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:omnibox::kIsOmniboxInBottomPosition];
    [_bottomOmniboxPref setObserver:self];
  }
  return self;
}

- (void)disconnect {
  [_bottomOmniboxPref stop];
  _bottomOmniboxPref = nil;
}

- (BOOL)isOmniboxInBottomPosition {
  return IsBottomOmniboxAvailable() && _bottomOmniboxPref.value;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxPref) {
    [self.delegate mainToolbarMediatorDidChangeOmniboxPosition:self];
  }
}

@end
