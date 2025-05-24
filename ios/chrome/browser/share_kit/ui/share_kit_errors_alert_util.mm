// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/ui/share_kit_errors_alert_util.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace share_kit {

namespace {

// Returns an UIAlertController for the given parameters.
UIAlertController* ConfigureAlertController(
    int title,
    int message,
    int action_title,
    void (^action_handler)(UIAlertAction*)) {
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(title)
                       message:l10n_util::GetNSString(message)
                preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* action =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(action_title)
                               style:UIAlertActionStyleDefault
                             handler:action_handler];
  [alert addAction:action];
  return alert;
}

}  // namespace

UIAlertController* InvalidLinkAlert() {
  return ConfigureAlertController(
      IDS_IOS_SHARE_KIT_JOIN_ALERT_EXPIRED_TITLE,
      IDS_IOS_SHARE_KIT_JOIN_ALERT_EXPIRED_MESSAGE,
      IDS_IOS_SHARE_KIT_JOIN_ALERT_EXPIRED_ACTION_TITLE,
      /*action_handler*/ nil);
}

}  // namespace share_kit
