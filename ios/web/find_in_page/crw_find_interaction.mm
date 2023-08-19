// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/find_in_page/crw_find_interaction.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/web/public/find_in_page/crw_find_session.h"

@implementation CRWFindInteraction {
  // Underlying UIFindInteraction object.
  UIFindInteraction* _UIFindInteraction;
}

- (instancetype)initWithUIFindInteraction:
    (UIFindInteraction*)UIFindInteraction {
  DCHECK(UIFindInteraction);
  self = [super init];
  if (self) {
    _UIFindInteraction = UIFindInteraction;
  }
  return self;
}

- (BOOL)isFindNavigatorVisible {
  return [_UIFindInteraction isFindNavigatorVisible];
}

- (id<CRWFindSession>)activeFindSession {
  UIFindSession* UIFindSession = [_UIFindInteraction activeFindSession];
  return [[CRWFindSession alloc] initWithUIFindSession:UIFindSession];
}

- (NSString*)searchText {
  return [_UIFindInteraction searchText];
}

- (void)setSearchText:(NSString*)searchText {
  [_UIFindInteraction setSearchText:searchText];
}

- (void)presentFindNavigatorShowingReplace:(BOOL)showingReplace {
  [_UIFindInteraction presentFindNavigatorShowingReplace:showingReplace];
}

- (void)dismissFindNavigator {
  [_UIFindInteraction dismissFindNavigator];
}

@end
