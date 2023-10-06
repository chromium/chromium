// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_password_infobar_metrics_recorder.h"

#import "base/metrics/histogram_macros.h"

namespace {

// Histogram names for PasswordInfobarTypeSave.
// Modal.
const char kPasswordInfobarSaveModalEventHistogram[] =
    "Mobile.Messages.Passwords.Modal.Event.InfobarTypePasswordSave";
const char kPasswordInfobarSaveModalDismissHistogram[] =
    "Mobile.Messages.Passwords.Modal.Dismiss.InfobarTypePasswordSave";
const char kPasswordInfobarSaveModalPresentHistogram[] =
    "Mobile.Messages.Passwords.Modal.Present.InfobarTypePasswordSave";

// Histogram names for PasswordInfobarTypeUpdate.
// Modal.
const char kPasswordInfobarUpdateModalEventHistogram[] =
    "Mobile.Messages.Passwords.Modal.Event.InfobarTypePasswordUpdate";
const char kPasswordInfobarUpdateModalDismissHistogram[] =
    "Mobile.Messages.Passwords.Modal.Dismiss.InfobarTypePasswordUpdate";
const char kPasswordInfobarUpdateModalPresentHistogram[] =
    "Mobile.Messages.Passwords.Modal.Present.InfobarTypePasswordUpdate";

}  // namespace

@interface IOSChromePasswordInfobarMetricsRecorder ()
// The Password Infobar type for the metrics recorder.
@property(nonatomic, assign) PasswordInfobarType passwordInfobarType;
@end

@implementation IOSChromePasswordInfobarMetricsRecorder

- (instancetype)initWithType:(PasswordInfobarType)passwordInfobarType {
  self = [super init];
  if (self) {
    _passwordInfobarType = passwordInfobarType;
  }
  return self;
}

- (void)recordModalEvent:(MobileMessagesPasswordsModalEvent)event {
  switch (self.passwordInfobarType) {
    case PasswordInfobarType::kPasswordInfobarTypeSave:
      UMA_HISTOGRAM_ENUMERATION(kPasswordInfobarSaveModalEventHistogram, event);
      break;
    case PasswordInfobarType::kPasswordInfobarTypeUpdate:
      UMA_HISTOGRAM_ENUMERATION(kPasswordInfobarUpdateModalEventHistogram,
                                event);
      break;
  }
}

- (void)recordModalDismiss:(MobileMessagesPasswordsModalDismiss)dismissType {
  switch (self.passwordInfobarType) {
    case PasswordInfobarType::kPasswordInfobarTypeSave:
      UMA_HISTOGRAM_ENUMERATION(kPasswordInfobarSaveModalDismissHistogram,
                                dismissType);
      break;
    case PasswordInfobarType::kPasswordInfobarTypeUpdate:
      UMA_HISTOGRAM_ENUMERATION(kPasswordInfobarUpdateModalDismissHistogram,
                                dismissType);
      break;
  }
}

- (void)recordModalPresent:(MobileMessagesPasswordsModalPresent)presentContext {
  switch (self.passwordInfobarType) {
    case PasswordInfobarType::kPasswordInfobarTypeSave:
      UMA_HISTOGRAM_ENUMERATION(kPasswordInfobarSaveModalPresentHistogram,
                                presentContext);
      break;
    case PasswordInfobarType::kPasswordInfobarTypeUpdate:
      UMA_HISTOGRAM_ENUMERATION(kPasswordInfobarUpdateModalPresentHistogram,
                                presentContext);
      break;
  }
}

@end
