// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_window_ios_factory.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "ios/chrome/browser/sessions/model/web_state_list_serialization.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@interface SessionWindowIOSFactory ()
// Returns YES if the current session can be saved.
- (BOOL)canSaveCurrentSession;
@end

@implementation SessionWindowIOSFactory {
  raw_ptr<WebStateList> _webStateList;
}

#pragma mark - Initialization

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  if ((self = [super init])) {
    DCHECK(webStateList);
    _webStateList = webStateList;
  }
  return self;
}

#pragma mark - Public

- (void)disconnect {
  _webStateList = nullptr;
}

- (SessionWindowIOS*)sessionForSaving {
  if (![self canSaveCurrentSession]) {
    return nil;
  }
  // Build the array of sessions. Copy the session objects as the saving will
  // be done on a separate thread.
  return SerializeWebStateList(_webStateList);
}

#pragma mark - Private

- (BOOL)canSaveCurrentSession {
  // The `_webStateList` needs to be alive for the session to be saved.
  if (!_webStateList) {
    return NO;
  }

  // Sessions where there's no active tab shouldn't be saved, unless the web
  // state list is empty. This is a transitional state.
  if (!_webStateList->empty() && !_webStateList->GetActiveWebState()) {
    return NO;
  }

  return YES;
}

@end
