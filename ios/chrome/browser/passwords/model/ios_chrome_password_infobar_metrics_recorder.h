// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_INFOBAR_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_INFOBAR_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

// Password Infobars types. Since these are used for metrics, entries should not
// be renumbered and numeric values should never be reused.
enum class PasswordInfobarType {
  // Message Infobar for Saving a password.
  kPasswordInfobarTypeSave = 0,
  // Message Infobar for Updating a password.
  kPasswordInfobarTypeUpdate = 1,
};

// Values for the UMA Mobile.Messages.Passwords.Modal.Event histogram. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class MobileMessagesPasswordsModalEvent {
  // PasswordInfobar username was edited.
  EditedUserName = 0,
  // PasswordInfobar password was edited.
  EditedPassword = 1,
  // PasswordInfobar password was unmasked.
  UnmaskedPassword = 2,
  // PasswordInfobar password was masked.
  MaskedPassword = 3,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = MaskedPassword,
};

// Values for the UMA Mobile.Messages.Passwords.Modal.Dismiss histogram. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class MobileMessagesPasswordsModalDismiss {
  // PasswordInfobar was tapped on Never For This Site.
  TappedNeverForThisSite = 0,
  // PasswordInfobar credentials were saved.
  SavedCredentials = 1,
  // PasswordInfobar credentials were updated.
  UpdatedCredentials = 2,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = UpdatedCredentials,
};

// Values for the UMA Mobile.Messages.Passwords.Modal.Present histogram. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class MobileMessagesPasswordsModalPresent {
  // PasswordInfobar was presented after a Save Password banner was
  // presented.
  PresentedAfterSaveBanner = 0,
  // PasswordInfobar was presented after an Update Password banner was
  // presented.
  PresentedAfterUpdateBanner = 1,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = PresentedAfterUpdateBanner,
};

// Used to record metrics related to Password Infobar events.
@interface IOSChromePasswordInfobarMetricsRecorder : NSObject

// Designated initializer. IOSChromePasswordInfobarMetricsRecorder will record
// metrics for `passwordInfobarType`.
- (instancetype)initWithType:(PasswordInfobarType)passwordInfobarType
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Records histogram for Modal `event`.
- (void)recordModalEvent:(MobileMessagesPasswordsModalEvent)event;

// Records histogram for Modal `dismissType`.
- (void)recordModalDismiss:(MobileMessagesPasswordsModalDismiss)dismissType;

// Records histogram for Modal `presentContext`.
- (void)recordModalPresent:(MobileMessagesPasswordsModalPresent)presentContext;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_INFOBAR_METRICS_RECORDER_H_
