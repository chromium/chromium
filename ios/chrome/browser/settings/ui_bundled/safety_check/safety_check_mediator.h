// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/scoped_refptr.h"
#import "ios/chrome/browser/settings/ui_bundled/safety_check/safety_check_mediator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/safety_check/safety_check_service_delegate.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// Webpage with safe browsing toggle.
extern const char kSafeBrowsingStringURL[];

namespace password_manager {
enum class PasswordCheckReferrer;
}  // namespace password_manager
namespace syncer {
class SyncService;
}  // namespace syncer

class AuthenticationService;
class IOSChromePasswordCheckManager;
class PrefService;
@protocol SafetyCheckConsumer;
@protocol SafetyCheckNavigationCommands;

@class SafetyCheckTableViewController;

// Enum representing the different item types used in the Safety Check UI.
// These item types correspond to various sections and functionalities
// displayed within the Safety Check feature, including update checks,
// password checks, Safe Browsing status, and notifications opt-in settings.
//
// The values are grouped as follows:
// - CheckTypes section: Items related to checking updates, passwords, and Safe
// Browsing status.
// - CheckStart section: Items related to starting or stopping the Safety Check
// process.
// - Notifications opt-in section: Items for managing notifications preferences.
typedef NS_ENUM(NSInteger, SafetyCheckItemType) {
  // CheckTypes section.
  UpdateItemType = kItemTypeEnumZero,
  PasswordItemType,
  SafeBrowsingItemType,
  HeaderItem,
  // CheckStart section.
  CheckStartItemType,
  TimestampFooterItem,
  // Notifications opt-in section.
  NotificationsOptInItemType,
};

// The mediator is pushing the data for the safety check to the consumer.
@interface SafetyCheckMediator : NSObject <SafetyCheckServiceDelegate>

// Designated initializer. All the parameters should not be null.
// `userPrefService`: Preference service to access safe browsing state.
// `localPrefService`: Preference service from the application context.
// `passwordCheckManager`: Password check manager to enable use of the password
// check service.
// `authService`: Authentication service to check users authentication status.
// `syncService`: Sync service to check sync and sync encryption status.
// `referrer`: Where in the app the Safety Check is being requested from.
- (instancetype)
    initWithUserPrefService:(PrefService*)userPrefService
           localPrefService:(PrefService*)localPrefService
       passwordCheckManager:
           (scoped_refptr<IOSChromePasswordCheckManager>)passwordCheckManager
                authService:(AuthenticationService*)authService
                syncService:(syncer::SyncService*)syncService
                   referrer:(password_manager::PasswordCheckReferrer)referrer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Must be called before -dealloc.
- (void)disconnect;

// Starts a safety check if one is not currently running.
- (void)startCheckIfNotRunning;

// Updates the display of the notifications opt-in section based on whether push
// notifications are `enabled`.
- (void)reconfigureNotificationsSection:(BOOL)enabled;

// The consumer for the Safety Check mediator.
@property(nonatomic, weak) id<SafetyCheckConsumer> consumer;

// Handler used to navigate inside the safety check.
@property(nonatomic, weak) id<SafetyCheckNavigationCommands> handler;

// Delegate used to communicate events back to the owner of this class.
@property(nonatomic, weak) id<SafetyCheckMediatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_
