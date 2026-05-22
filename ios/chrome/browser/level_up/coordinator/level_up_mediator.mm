// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/level_up/ui/level_up_consumer.h"
#import "ios/chrome/browser/level_up/ui/level_up_profile_consumer.h"
#import "ios/chrome/browser/level_up/ui/level_up_task.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/avatar/resized_avatar_cache.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

@implementation LevelUpMediator {
  // The authentication service.
  raw_ptr<AuthenticationService> _authService;
  // Image cache for user avatars.
  ResizedAvatarCache* _avatarCache;
}

- (instancetype)initWithAuthenticationService:
    (AuthenticationService*)authService {
  self = [super init];
  if (self) {
    _authService = authService;
    _avatarCache = [[ResizedAvatarCache alloc]
        initWithIdentityAvatarSize:IdentityAvatarSize::Large];
  }
  return self;
}

- (void)setConsumer:(id<LevelUpConsumer>)consumer {
  _consumer = consumer;

  id<SystemIdentity> identity = _authService->GetPrimaryIdentity();

  int level = 1;
  NSString* userFullName = identity.userFullName;
  UIImage* userAvatar = [_avatarCache resizedAvatarForIdentity:identity];

  LevelUpTask* task1 = [[LevelUpTask alloc]
        initWithTaskID:@"default_browser"
                 title:@"Set Chrome as Default Browser"
       taskDescription:@"Use Chrome as your primary default browser"
        iconSymbolName:kDefaultBrowserSymbol
             completed:YES
      navigationAction:^{
      }];
  LevelUpTask* task2 =
      [[LevelUpTask alloc] initWithTaskID:@"bookmarks_sync"
                                    title:@"Sync your bookmarks"
                          taskDescription:@"Access your bookmarks on any device"
                           iconSymbolName:kBookmarksSymbol
                                completed:YES
                         navigationAction:^{
                         }];
  LevelUpTask* task3 = [[LevelUpTask alloc]
        initWithTaskID:@"password_manager"
                 title:@"Enable Password Manager"
       taskDescription:@"Check and autofill your passwords securely"
        iconSymbolName:kKeySymbol
             completed:YES
      navigationAction:^{
      }];
  LevelUpTask* task4 = [[LevelUpTask alloc]
        initWithTaskID:@"widgets"
                 title:@"Add widgets to Home Screen"
       taskDescription:@"Access search and bookmarks from the Home Screen"
        iconSymbolName:kPlusInSquareSymbol
             completed:NO
      navigationAction:^{
      }];

  [self.consumer setLevel:level tasksForLevel:@[ task1, task2, task3, task4 ]];
  [self.profileConsumer setUserFullName:userFullName userAvatar:userAvatar];
}

@end
