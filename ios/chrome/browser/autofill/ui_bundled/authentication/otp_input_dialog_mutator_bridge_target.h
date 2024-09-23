// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MUTATOR_BRIDGE_TARGET_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MUTATOR_BRIDGE_TARGET_H_

#import <string>

// The C++ equivalent interface of the Objective-C protocol,
// OtpInputDialogMutator.
class OtpInputDialogMutatorBridgeTarget {
 public:
  virtual void DidTapConfirmButton(const std::u16string& input_value) = 0;
  virtual void DidTapCancelButton() = 0;
  virtual void OnOtpInputChanges(const std::u16string& input_value) = 0;
  virtual void DidTapNewCodeLink() = 0;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MUTATOR_BRIDGE_TARGET_H_
