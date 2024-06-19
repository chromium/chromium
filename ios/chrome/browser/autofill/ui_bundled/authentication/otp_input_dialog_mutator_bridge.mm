// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mutator_bridge.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mutator_bridge_target.h"

@implementation OtpInputDialogMutatorBridge {
  base::WeakPtr<OtpInputDialogMutatorBridgeTarget> _target;
}

- (instancetype)initWithTarget:
    (base::WeakPtr<OtpInputDialogMutatorBridgeTarget>)target {
  self = [super init];
  if (self) {
    _target = target;
  }
  return self;
}

#pragma mark - OtpInputDialogMutator

- (void)didTapConfirmButton:(NSString*)inputValue {
  if (_target) {
    _target->DidTapConfirmButton(base::SysNSStringToUTF16(inputValue));
  }
}

- (void)didTapCancelButton {
  if (_target) {
    _target->DidTapCancelButton();
  }
}

- (void)onOtpInputChanges:(NSString*)inputValue {
  if (_target) {
    _target->OnOtpInputChanges(base::SysNSStringToUTF16(inputValue));
  }
}

- (void)didTapNewCodeLink {
  if (_target) {
    _target->DidTapNewCodeLink();
  }
}

@end
