// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/scoped_refptr.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_service_delegate.h"

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

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_
