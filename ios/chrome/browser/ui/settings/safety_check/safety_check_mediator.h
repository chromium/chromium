// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_service_delegate.h"

#include "base/memory/scoped_refptr.h"

#import <UIKit/UIKit.h>

// Webpage with safe browsing toggle.
extern const char kSafeBrowsingStringURL[];

class AuthenticationService;
class IOSChromePasswordCheckManager;
class PrefService;
@protocol SafetyCheckConsumer;
@protocol SafetyCheckNavigationCommands;
class SyncSetupService;

@class SafetyCheckTableViewController;

// The mediator is pushing the data for the safety check to the consumer.
@interface SafetyCheckMediator : NSObject <SafetyCheckServiceDelegate>

// Designated initializer. All the parameters should not be null.
// `userPrefService`: Preference service to access safe browsing state.
// `passwordCheckManager`: Password check manager to enable use of the password
// check service.
// `authService`: Authentication service to check users authentication status.
// `syncService`: Sync service to check sync and sync encryption status.
- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
                   passwordCheckManager:
                       (scoped_refptr<IOSChromePasswordCheckManager>)
                           passwordCheckManager
                            authService:(AuthenticationService*)authService
                            syncService:(SyncSetupService*)syncService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts a safety check if one is not currently running.
- (void)startCheckIfNotRunning;

// The consumer for the Safety Check mediator.
@property(nonatomic, weak) id<SafetyCheckConsumer> consumer;

// Handler used to navigate inside the safety check.
@property(nonatomic, weak) id<SafetyCheckNavigationCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_
