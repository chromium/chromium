// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"

// Objective-C protocol mirroring `IOSChromeSafetyCheckManagerObserver`.
@protocol SafetyCheckManagerObserver <NSObject>

// Called whenever the Safety Check determines a change in the Password check
// state (i.e. when user has reused passwords, weak passwords, no compromised
// password, etc.), and includes the latest `insecurePasswordCounts`.
- (void)passwordCheckStateChanged:(PasswordSafetyCheckState)state
           insecurePasswordCounts:
               (password_manager::InsecurePasswordCounts)insecurePasswordCounts;

// Called whenever the Safety Check determines a change in the Safe Browsing
// check state (i.e. when Safe Browsing is enabled, disabled, the check
// is currently running, etc.)
- (void)safeBrowsingCheckStateChanged:(SafeBrowsingSafetyCheckState)state;

// Called whenever the Safety Check determines a change in the Update Chrome
// check state (i.e. when Chrome is up to date, Chrome is out of date, the
// check is currently running, etc.)
- (void)updateChromeCheckStateChanged:(UpdateChromeSafetyCheckState)state;

// Called whenever the Safety Check begins the async process of evaluating the
// Password check, Safe Browsing check, and/or Update check.
- (void)runningStateChanged:(RunningSafetyCheckState)state;

// Notifies the observer that the Safety Check Manager has begun shutting down.
// Observers should reset their `SafetyCheckObserverBridge` observation when
// this happens.
- (void)safetyCheckManagerWillShutdown;

@end

// Simple observer bridge that forwards all events to its delegate observer.
class SafetyCheckObserverBridge : public IOSChromeSafetyCheckManagerObserver {
 public:
  SafetyCheckObserverBridge(id<SafetyCheckManagerObserver> delegate,
                            IOSChromeSafetyCheckManager* manager);

  ~SafetyCheckObserverBridge() override;

  void PasswordCheckStateChanged(PasswordSafetyCheckState state,
                                 password_manager::InsecurePasswordCounts
                                     insecure_password_counts) override;
  void SafeBrowsingCheckStateChanged(
      SafeBrowsingSafetyCheckState state) override;
  void UpdateChromeCheckStateChanged(
      UpdateChromeSafetyCheckState state) override;
  void RunningStateChanged(RunningSafetyCheckState state) override;
  void ManagerWillShutdown(
      IOSChromeSafetyCheckManager* safety_check_manager) override;

 private:
  __weak id<SafetyCheckManagerObserver> delegate_ = nil;

  base::ScopedObservation<IOSChromeSafetyCheckManager,
                          IOSChromeSafetyCheckManagerObserver>
      safety_check_manager_observation_{this};

  raw_ptr<IOSChromeSafetyCheckManager> safety_check_manager_;
};

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_OBSERVER_BRIDGE_H_
