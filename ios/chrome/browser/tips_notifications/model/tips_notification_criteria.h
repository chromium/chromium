// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_CRITERIA_H_
#define IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_CRITERIA_H_

#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"

class PrefService;
class ProfileIOS;
enum class TipsNotificationType;
enum class TipsNotificationUserType;

// A class that can evaluate the criteria for sending each type of Tips
// Notification.
class TipsNotificationCriteria {
 public:
  // Constructs a `TipsNotificationCriteria` object with the given `profile`
  // and `local_state`. `reactivation` indicates whether the app is sending
  // provisional "reactivation" / "proactive" tips notifications.
  TipsNotificationCriteria(ProfileIOS* profile,
                           PrefService* local_state,
                           bool reactivation = false);

  // Returns true if the given `type` of notification should be sent.
  bool ShouldSendNotification(TipsNotificationType type);

  // Returns true if the user is allowed to sign in and isn't currently signed
  // in.
  static bool CanSignIn(ProfileIOS* profile);

 private:
  // Helpers that evaluate the criteria for sending each type of Tips
  // Notification. If they return true, that type of notification is
  // eligible to be sent.
  bool ShouldSendDefaultBrowser();
  bool ShouldSendSignin();
  bool ShouldSendWhatsNew();
  bool ShouldSendSetUpListContinuation();
  bool ShouldSendDocking();
  bool ShouldSendOmniboxPosition();
  bool ShouldSendLens();
  bool ShouldSendEnhancedSafeBrowsing();
  bool ShouldSendCPE();
  bool ShouldSendLensOverlay();
  bool ShouldSendTrustedVaultKeyRetrieval();

  // Returns true if the Feature Engagement Tracker has ever triggered for the
  // given `feature`.
  bool FETHasEverTriggered(const base::Feature& feature);

  // Stores the profile that will be used to evaluate criteria.
  raw_ptr<ProfileIOS> profile_;

  // Stores the prefs for the profile.
  raw_ptr<PrefService> profile_prefs_;

  // Stores the local state prefs.
  raw_ptr<PrefService> local_state_;

  // True if the criteria are being evaluated for reactivation notifications.
  bool reactivation_;
};

#endif  // IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_CRITERIA_H_
