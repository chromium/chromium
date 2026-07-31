// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_category.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_stat.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/task_types.h"
#import "ios/chrome/browser/level_up/ui/level_up_consumer.h"
#import "ios/chrome/browser/level_up/ui/level_up_profile_consumer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/avatar/avatar_provider.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface LevelUpMediator () <IdentityManagerObserving, PrefObserverDelegate>
@end

@implementation LevelUpMediator {
  // The authentication service.
  raw_ptr<AuthenticationService> _authService;
  // The identity manager.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Bridge to register for IdentityManager changes.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  // The level up service.
  raw_ptr<LevelUpService> _levelUpService;
  // The pref service.
  raw_ptr<PrefService> _prefService;
  // The currently displayed identity.
  id<SystemIdentity> _currentIdentity;
  // The list of task categories.
  NSArray<LevelUpCategory*>* _categories;

  // Registrar for user Pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Bridge to listen to Pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager
                   levelUpService:(LevelUpService*)levelUpService
                      prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    CHECK(authService);
    CHECK(identityManager);
    _authService = authService;
    _identityManager = identityManager;
    _identityManagerObserverBridge =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _levelUpService = levelUpService;
    _prefService = prefService;
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge->ObserveChangesForPreference(prefs::kLevelUpUIEnabled,
                                                     &_prefChangeRegistrar);
  }
  return self;
}

- (void)setProfileConsumer:(id<LevelUpProfileConsumer>)profileConsumer {
  _profileConsumer = profileConsumer;

  // Skip doing unnecessary work if there's no consumer for it.
  if (!self.profileConsumer) {
    return;
  }

  [self updateProfileInfo];
}

- (void)setConsumer:(id<LevelUpConsumer>)consumer {
  _consumer = consumer;

  // Skip doing unnecessary work if there's no consumer for it.
  if (!self.consumer) {
    return;
  }

  if ([self.consumer
          respondsToSelector:@selector(setProgressUpdatesEnabled:)]) {
    BOOL updatesEnabled = _prefService->GetBoolean(prefs::kLevelUpUIEnabled);
    [self.consumer setProgressUpdatesEnabled:updatesEnabled];
  }

  int level = _levelUpService->GetCurrentLevel();

  NSMutableArray<LevelUpTask*>* productivityTasks =
      [[NSMutableArray alloc] init];
  NSMutableArray<LevelUpTask*>* safetyTasks = [[NSMutableArray alloc] init];
  NSMutableArray<LevelUpTask*>* searchTasks = [[NSMutableArray alloc] init];
  NSMutableArray<LevelUpTask*>* allTasks = [[NSMutableArray alloc] init];

  const auto& tasks = _levelUpService->GetTasks();
  for (const auto& [type, info] : tasks) {
    BOOL completed = _levelUpService->IsTaskCompleted(type);
    LevelUpTask* task = [[LevelUpTask alloc] initWithTaskInfo:info.get()
                                                    completed:completed];
    [allTasks addObject:task];

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

  // TODO(crbug.com/523325903): Update this to match the final design spec. Use
  // the first 4 uncompleted tasks for now.
  NSMutableArray<LevelUpTask*>* uncompletedTasks =
      [[NSMutableArray alloc] init];
  for (LevelUpTask* task in allTasks) {
    if (!task.completed) {
      [uncompletedTasks addObject:task];
      if (uncompletedTasks.count == 4) {
        break;
      }
    }
  }
  NSArray<LevelUpTask*>* tasksForCurrentLevel = uncompletedTasks;

  if ([self.consumer respondsToSelector:@selector(setLevel:tasksForLevel:)]) {
    [self.consumer setLevel:level tasksForLevel:tasksForCurrentLevel];
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
}

- (void)configureAllTasksConsumer:(id<LevelUpConsumer>)allTasksConsumer {
  if ([allTasksConsumer respondsToSelector:@selector(addCategoryCard:)]) {
    for (LevelUpCategory* category in _categories) {
      [allTasksConsumer addCategoryCard:category];
    }
  }
}

- (void)disconnect {
  _identityManagerObserverBridge.reset();
  _prefObserverBridge.reset();
  _prefChangeRegistrar.RemoveAll();
  _authService = nullptr;
  _identityManager = nullptr;
  _levelUpService = nullptr;
  _prefService = nullptr;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kLevelUpUIEnabled) {
    BOOL updatesEnabled = _prefService->GetBoolean(prefs::kLevelUpUIEnabled);
    if ([self.consumer
            respondsToSelector:@selector(setProgressUpdatesEnabled:)]) {
      [self.consumer setProgressUpdatesEnabled:updatesEnabled];
    }
  }
}

#pragma mark - Private

// Configures the task stat.
- (void)configureTaskStat:(NSArray<LevelUpTask*>*)allTasks {
  NSMutableArray<LevelUpStat*>* stats = [[NSMutableArray alloc] init];

  int tabsDecluttered =
      _levelUpService->GetStatValue(LevelUpTaskStatType::kTabsDecluttered);
  NSString* title1 = l10n_util::GetPluralNSStringF(
      IDS_IOS_LEVEL_UP_STAT_TABS_DECLUTTERED, tabsDecluttered);
  NSString* subtitle1 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_TABS_DECLUTTERED);
  LevelUpStat* stat1 =
      [[LevelUpStat alloc] initWithTitle:title1
                                subtitle:subtitle1
                         imageLottieName:@"tabs_decluttered"
                                    type:LevelUpTaskStatType::kTabsDecluttered];
  [stats addObject:stat1];

  int typingSaved =
      _levelUpService->GetStatValue(LevelUpTaskStatType::kTypingSaved);
  NSString* title2 = l10n_util::GetPluralNSStringF(
      IDS_IOS_LEVEL_UP_STAT_TYPING_SAVED, typingSaved);
  NSString* subtitle2 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_TYPING_SAVED);
  LevelUpStat* stat2 =
      [[LevelUpStat alloc] initWithTitle:title2
                                subtitle:subtitle2
                         imageLottieName:@"typing_saved"
                                    type:LevelUpTaskStatType::kTypingSaved];
  [stats addObject:stat2];

  int passwordsVerified =
      _levelUpService->GetStatValue(LevelUpTaskStatType::kPasswordsVerified);
  NSString* title3 = l10n_util::GetPluralNSStringF(
      IDS_IOS_LEVEL_UP_STAT_PASSWORDS_VERIFIED, passwordsVerified);
  NSString* subtitle3 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_PASSWORDS_VERIFIED);
  LevelUpStat* stat3 = [[LevelUpStat alloc]
        initWithTitle:title3
             subtitle:subtitle3
      imageLottieName:@"passwords_verified"
                 type:LevelUpTaskStatType::kPasswordsVerified];
  [stats addObject:stat3];

  int photoSearchesPerformed = _levelUpService->GetStatValue(
      LevelUpTaskStatType::kPhotoSearchesPerformed);
  NSString* title4 = l10n_util::GetPluralNSStringF(
      IDS_IOS_LEVEL_UP_STAT_PHOTO_SEARCHES_PERFORMED, photoSearchesPerformed);
  NSString* subtitle4 = l10n_util::GetNSString(
      IDS_IOS_LEVEL_UP_STAT_SUBTITLE_PHOTO_SEARCHES_PERFORMED);
  LevelUpStat* stat4 = [[LevelUpStat alloc]
        initWithTitle:title4
             subtitle:subtitle4
      imageLottieName:@"photo_searches_performed"
                 type:LevelUpTaskStatType::kPhotoSearchesPerformed];
  [stats addObject:stat4];

  if ([self.consumer respondsToSelector:@selector(setStats:)]) {
    [self.consumer setStats:stats];
  }
}

- (void)toggleProgressUpdates {
  BOOL oldValue = _prefService->GetBoolean(prefs::kLevelUpUIEnabled);
  BOOL newValue = !oldValue;
  _prefService->SetBoolean(prefs::kLevelUpUIEnabled, newValue);
  if ([self.consumer
          respondsToSelector:@selector(setProgressUpdatesEnabled:)]) {
    [self.consumer setProgressUpdatesEnabled:newValue];
  }
}

// Updates the profile consumer with the primary identity credentials.
- (void)updateProfileInfo {
  id<SystemIdentity> identity = _authService->GetPrimaryIdentity();
  if (_currentIdentity == identity) {
    return;
  }
  _currentIdentity = identity;

  if (!identity) {
    [self.delegate levelUpMediatorWantsToBeDismissed:self];
    return;
  }

  NSString* userFullName = identity.userFullName;
  UIImage* userAvatar =
      GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
          identity, IdentityAvatarSize::Large);
  [self.profileConsumer setUserFullName:userFullName userAvatar:userAvatar];
}

#pragma mark - IdentityManagerObserving

- (void)primaryAccountDidChange:
    (const signin::PrimaryAccountChangeEvent&)event {
  if (_identityManager->IsBatchOfPrimaryAccountChangesInProgress()) {
    return;
  }
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateProfileInfo];
      break;
  }
}

- (void)extendedAccountInfoDidUpdate:(const AccountInfo&)info {
  if (_identityManager->IsBatchOfPrimaryAccountChangesInProgress()) {
    return;
  }
  [self updateProfileInfo];
}

@end
