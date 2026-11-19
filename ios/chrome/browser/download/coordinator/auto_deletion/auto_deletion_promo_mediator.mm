// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_promo_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_promo_commands.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation AutoDeletionPromoMediator {
  // A pointer to the `ApplicationContext::LocalState` `PrefService`.
  raw_ptr<PrefService> _localState;
}

- (instancetype)initWithLocalState:(PrefService*)localState {
  self = [super init];
  if (self) {
    CHECK(localState);
    _localState = localState;
  }

  return self;
}

#pragma mark - Public

- (void)disconnect {
  _localState = nullptr;
  self.autoDeletionPromoHandler = nil;
}

#pragma mark - AutoDeletionPromoMutator

- (void)enableAutoDeletion {
  if (!_localState) {
    return;
  }
  _localState->SetBoolean(prefs::kDownloadAutoDeletionEnabled, true);
  [self.autoDeletionPromoHandler presentAutoDeletionActionSheet];
}

@end
