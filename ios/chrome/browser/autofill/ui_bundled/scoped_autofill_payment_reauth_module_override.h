// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_SCOPED_AUTOFILL_PAYMENT_REAUTH_MODULE_OVERRIDE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_SCOPED_AUTOFILL_PAYMENT_REAUTH_MODULE_OVERRIDE_H_

#import <memory>

@protocol ReauthenticationProtocol;

// Util class enabling a global override of the Reauthentication Module used in
// Autofill payments flows, for testing purposes only.
class ScopedAutofillPaymentReauthModuleOverride {
 public:
  ~ScopedAutofillPaymentReauthModuleOverride();

  // Returns the override module, if one exists. This will be a
  // nullptr unless a override is created and set by the above
  // `MakeAndArmForTesting` call.
  static id<ReauthenticationProtocol> Get();

  // Creates a scoped override so that the provided reauthentication module will
  // be used in place of the production implementation.
  static std::unique_ptr<ScopedAutofillPaymentReauthModuleOverride>
  MakeAndArmForTesting(id<ReauthenticationProtocol> module);

  // The module to be used.
  id<ReauthenticationProtocol> module;

 private:
  ScopedAutofillPaymentReauthModuleOverride() = default;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_SCOPED_AUTOFILL_PAYMENT_REAUTH_MODULE_OVERRIDE_H_
