// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_state.h"

#import "base/check.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

@implementation ProfileState {
  base::WeakPtr<ChromeBrowserState> _browserState;
}

#pragma mark - Properties

- (ChromeBrowserState*)browserState {
  return _browserState.get();
}

- (void)setBrowserState:(ChromeBrowserState*)browserState {
  CHECK(browserState);
  _browserState = browserState->AsWeakPtr();
}

@end
