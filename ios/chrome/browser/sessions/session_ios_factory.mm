// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_ios_factory.h"

#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_serialization.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SessionIOSFactory ()
// Returns YES if the current session can be saved.
- (BOOL)canSaveCurrentSession;
@end

@implementation SessionIOSFactory {
  WebStateList* _webStateList;
  NSMutableSet<NSString*>* _dirtyWebStates;
}

#pragma mark - Initialization

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  if (self = [super init]) {
    DCHECK(webStateList);
    _webStateList = webStateList;
    _dirtyWebStates = [[NSMutableSet alloc] init];
  }
  return self;
}

#pragma mark - Public

- (void)disconnect {
  _webStateList = nullptr;
}

- (SessionIOS*)sessionForSaving {
  if (![self canSaveCurrentSession])
    return nil;
  // Build the array of sessions. Copy the session objects as the saving will
  // be done on a separate thread.
  // TODO(crbug.com/661986): This could get expensive especially since this
  // window may never be saved (if another call comes in before the delay).
  SessionIOS* session = [[SessionIOS alloc]
      initWithWindows:@[ SerializeWebStateList(_webStateList,
                                               _dirtyWebStates) ]];
  [_dirtyWebStates removeAllObjects];
  return session;
}

- (void)markWebStateDirty:(web::WebState*)webState {
  NSString* webStateID = webState->GetStableIdentifier();
  [_dirtyWebStates addObject:webStateID];
}

#pragma mark - Private

- (BOOL)canSaveCurrentSession {
  // The `_webStateList` needs to be alive for the session to be saved.
  if (!_webStateList)
    return NO;

  // Sessions where there's no active tab shouldn't be saved, unless the web
  // state list is empty. This is a transitional state.
  if (!_webStateList->empty() && !_webStateList->GetActiveWebState())
    return NO;

  return YES;
}

@end
