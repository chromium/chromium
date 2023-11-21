// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_SCOPED_AUTOFILL_PAYMENT_REAUTH_MODULE_OVERRIDE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_SCOPED_AUTOFILL_PAYMENT_REAUTH_MODULE_OVERRIDE_H_

#import "base/memory/raw_ptr.h"

@protocol ReauthenticationProtocol;

// Util class enabling a global override of the Reauthentication Module used in
// Autofill payments flows, for testing purposes only.
class ScopedAutofillPaymentReauthModuleOverride {
 public:
  ~ScopedAutofillPaymentReauthModuleOverride();

  // Creates a scoped override so that the provided reauthentication module will
  // be used in place of the production implementation.
  static std::unique_ptr<ScopedAutofillPaymentReauthModuleOverride>
  MakeAndArmForTesting(id<ReauthenticationProtocol> module);

  // Singleton instance of this class. This will be a nullptr unless a override
  // is created and set by the above `MakeAndArmForTesting` call.
  static raw_ptr<ScopedAutofillPaymentReauthModuleOverride> instance;

  // The module to be used.
  id<ReauthenticationProtocol> module;

 private:
  ScopedAutofillPaymentReauthModuleOverride() = default;
};

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_SCOPED_AUTOFILL_PAYMENT_REAUTH_MODULE_OVERRIDE_H_
