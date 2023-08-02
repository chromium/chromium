// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/crw_fake_find_interaction.h"

#import "ios/web/public/find_in_page/crw_find_session.h"

@implementation CRWFakeFindInteraction

@synthesize activeFindSession = _activeFindSession;
@synthesize findNavigatorVisible = _findNavigatorVisible;
@synthesize searchText = _searchText;

- (id<CRWFindSession>)activeFindSession {
  return _findNavigatorVisible ? _activeFindSession : nil;
}

- (void)presentFindNavigatorShowingReplace:(BOOL)showingReplace {
  _findNavigatorVisible = YES;
  if (_searchText) {
    [_activeFindSession performSearchWithQuery:_searchText options:nil];
  }
}

- (void)dismissFindNavigator {
  _findNavigatorVisible = NO;
}

@end
