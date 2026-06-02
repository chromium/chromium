// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_category.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_stat.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
#import "ios/chrome/browser/level_up/model/task_types.h"
#import "ios/chrome/browser/level_up/ui/level_up_consumer.h"
#import "ios/chrome/browser/level_up/ui/level_up_profile_consumer.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/avatar/resized_avatar_cache.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation LevelUpMediator {
  // The authentication service.
  raw_ptr<AuthenticationService> _authService;
  // Image cache for user avatars.
  ResizedAvatarCache* _avatarCache;
  // The list of task categories.
  NSArray<LevelUpCategory*>* _categories;
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
              category:LevelUpTaskCategory::kProductivity
      navigationAction:^{
      }];

  LevelUpTask* task2 = [[LevelUpTask alloc]
        initWithTaskID:@"password_autofill"
                 title:@"Save passwords and autofill"
       taskDescription:
           @"Quickly sign into sites and apps with your saved passwords"
        iconSymbolName:kKeySymbol
             completed:YES
              category:LevelUpTaskCategory::kProductivity
      navigationAction:^{
      }];

  LevelUpTask* task3 = [[LevelUpTask alloc]
        initWithTaskID:@"pin_tabs"
                 title:@"Pin tabs"
       taskDescription:@"Save your favorite sites by pinning them"
        iconSymbolName:kPlusInSquareSymbol
             completed:YES
              category:LevelUpTaskCategory::kProductivity
      navigationAction:^{
      }];

  LevelUpTask* task4 =
      [[LevelUpTask alloc] initWithTaskID:@"tab_group"
                                    title:@"Create a tab group"
                          taskDescription:@"Stay organized with tab groups"
                           iconSymbolName:kBookmarksSymbol
                                completed:NO
                                 category:LevelUpTaskCategory::kProductivity
                         navigationAction:^{
                         }];

  LevelUpTask* task5 = [[LevelUpTask alloc]
        initWithTaskID:@"gemini"
                 title:@"Use Gemini in Chrome"
       taskDescription:@"Get answers faster with Gemini in Chrome"
        iconSymbolName:kDefaultBrowserSymbol
             completed:NO
              category:LevelUpTaskCategory::kProductivity
      navigationAction:^{
      }];

  LevelUpTask* task6 = [[LevelUpTask alloc]
        initWithTaskID:@"payment_methods"
                 title:@"Manage payment methods"
       taskDescription:
           @"Add new payment methods or edit saved ones to check out faster"
        iconSymbolName:kKeySymbol
             completed:NO
              category:LevelUpTaskCategory::kProductivity
      navigationAction:^{
      }];

  // Safety category tasks.
  LevelUpTask* task7 = [[LevelUpTask alloc]
        initWithTaskID:@"quick_delete"
                 title:@"Quick delete"
       taskDescription:
           @"Manage your history, cookies and more to protect your privacy"
        iconSymbolName:kPlusInSquareSymbol
             completed:YES
              category:LevelUpTaskCategory::kSafety
      navigationAction:^{
      }];

  LevelUpTask* task8 = [[LevelUpTask alloc]
        initWithTaskID:@"safe_browsing"
                 title:@"Enhanced Safe Browsing"
       taskDescription:
           @"Add an extra layer of protection against online threats"
        iconSymbolName:kBookmarksSymbol
             completed:NO
              category:LevelUpTaskCategory::kSafety
      navigationAction:^{
      }];

  LevelUpTask* task9 = [[LevelUpTask alloc]
        initWithTaskID:@"incognito"
                 title:@"Go Incognito"
       taskDescription:@"Open incognito tabs to browse the web privately"
        iconSymbolName:kDefaultBrowserSymbol
             completed:NO
              category:LevelUpTaskCategory::kSafety
      navigationAction:^{
      }];

  // Search category tasks.
  LevelUpTask* task10 = [[LevelUpTask alloc]
        initWithTaskID:@"lens_search"
                 title:@"Search with Google Lens"
       taskDescription:@"Draw, highlight, or tap to search and get results "
                       @"without leaving your tab"
        iconSymbolName:kKeySymbol
             completed:YES
              category:LevelUpTaskCategory::kSearch
      navigationAction:^{
      }];

  LevelUpTask* task11 = [[LevelUpTask alloc]
        initWithTaskID:@"ai_search"
                 title:@"Search with AI Mode"
       taskDescription:@"Ask anything and get the best of the web"
        iconSymbolName:kPlusInSquareSymbol
             completed:NO
              category:LevelUpTaskCategory::kSearch
      navigationAction:^{
      }];

  LevelUpTask* task12 = [[LevelUpTask alloc]
        initWithTaskID:@"camera_search"
                 title:@"Search with camera"
       taskDescription:
           @"Shop, translate and identify what you see with your camera"
        iconSymbolName:kBookmarksSymbol
             completed:NO
              category:LevelUpTaskCategory::kSearch
      navigationAction:^{
      }];

  if ([self.consumer respondsToSelector:@selector(setLevel:tasksForLevel:)]) {
    [self.consumer setLevel:level
              tasksForLevel:@[ task1, task2, task3, task4 ]];
  }

  NSMutableArray* productivityTasks = [[NSMutableArray alloc] init];
  NSMutableArray* safetyTasks = [[NSMutableArray alloc] init];
  NSMutableArray* searchTasks = [[NSMutableArray alloc] init];

  NSArray<LevelUpTask*>* allTasks = @[
    task1, task2, task3, task4, task5, task6, task7, task8, task9, task10,
    task11, task12
  ];

  for (LevelUpTask* task in allTasks) {
    switch (task.category) {
      case LevelUpTaskCategory::kProductivity:
        [productivityTasks addObject:task];
        break;
      case LevelUpTaskCategory::kSafety:
        [safetyTasks addObject:task];
        break;
      case LevelUpTaskCategory::kSearch:
        [searchTasks addObject:task];
        break;
    }
  }

  _categories = @[
    [[LevelUpCategory alloc] initWithTitle:@"Productivity"
                                     tasks:productivityTasks],
    [[LevelUpCategory alloc] initWithTitle:@"Safety" tasks:safetyTasks],
    [[LevelUpCategory alloc] initWithTitle:@"Search" tasks:searchTasks]
  ];

  if ([self.consumer respondsToSelector:@selector(addCategoryCard:)]) {
    for (LevelUpCategory* category in _categories) {
      [self.consumer addCategoryCard:category];
    }
  }
  [self configureTaskStat:allTasks];

  [self.profileConsumer setUserFullName:userFullName userAvatar:userAvatar];
}

- (void)configureAllTasksConsumer:(id<LevelUpConsumer>)allTasksConsumer {
  if ([allTasksConsumer respondsToSelector:@selector(addCategoryCard:)]) {
    for (LevelUpCategory* category in _categories) {
      [allTasksConsumer addCategoryCard:category];
    }
  }
}

#pragma mark - Private

// Configures the task stat.
- (void)configureTaskStat:(NSArray<LevelUpTask*>*)allTasks {
  NSMutableArray<LevelUpStat*>* stats = [[NSMutableArray alloc] init];

  NSString* title1 =
      l10n_util::GetPluralNSStringF(IDS_IOS_LEVEL_UP_STAT_TABS_DECLUTTERED, 3);
  NSString* subtitle1 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_TABS_DECLUTTERED);
  LevelUpStat* stat1 = [[LevelUpStat alloc]
      initWithTitle:title1
           subtitle:subtitle1
              image:DefaultSymbolTemplateWithPointSize(kBookmarksSymbol, 28.0)
               type:LevelUpTaskStatType::kTabsDecluttered];
  [stats addObject:stat1];

  NSString* title2 =
      l10n_util::GetPluralNSStringF(IDS_IOS_LEVEL_UP_STAT_TYPING_SAVED, 5);
  NSString* subtitle2 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_TYPING_SAVED);
  LevelUpStat* stat2 = [[LevelUpStat alloc]
      initWithTitle:title2
           subtitle:subtitle2
              image:DefaultSymbolTemplateWithPointSize(kKeySymbol, 28.0)
               type:LevelUpTaskStatType::kTypingSaved];
  [stats addObject:stat2];

  NSString* title3 = l10n_util::GetPluralNSStringF(
      IDS_IOS_LEVEL_UP_STAT_PASSWORDS_VERIFIED, 5);
  NSString* subtitle3 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_PASSWORDS_VERIFIED);
  LevelUpStat* stat3 = [[LevelUpStat alloc]
      initWithTitle:title3
           subtitle:subtitle3
              image:DefaultSymbolTemplateWithPointSize(kKeySymbol, 28.0)
               type:LevelUpTaskStatType::kPasswordsVerified];
  [stats addObject:stat3];

  NSString* title4 =
      l10n_util::GetPluralNSStringF(IDS_IOS_LEVEL_UP_STAT_SEARCHES_SKIPPED, 3);
  NSString* subtitle4 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_SEARCHES_SKIPPED);
  LevelUpStat* stat4 =
      [[LevelUpStat alloc] initWithTitle:title4
                                subtitle:subtitle4
                                   image:DefaultSymbolTemplateWithPointSize(
                                             kDefaultBrowserSymbol, 28.0)
                                    type:LevelUpTaskStatType::kSearchesSkipped];
  [stats addObject:stat4];

  if ([self.consumer respondsToSelector:@selector(setStats:)]) {
    [self.consumer setStats:stats];
  }
}

@end
