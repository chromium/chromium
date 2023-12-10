// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/autofill/scoped_autofill_payment_reauth_module_override.h"

#import "base/check.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

// static
raw_ptr<ScopedAutofillPaymentReauthModuleOverride>
    ScopedAutofillPaymentReauthModuleOverride::instance;

// static
std::unique_ptr<ScopedAutofillPaymentReauthModuleOverride>
ScopedAutofillPaymentReauthModuleOverride::MakeAndArmForTesting(
    id<ReauthenticationProtocol> module) {
  CHECK(!instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedAutofillPaymentReauthModuleOverride> new_instance(
      new ScopedAutofillPaymentReauthModuleOverride);
  new_instance->module = module;
  instance = new_instance.get();
  return new_instance;
}

ScopedAutofillPaymentReauthModuleOverride::
    ~ScopedAutofillPaymentReauthModuleOverride() {
  CHECK(instance == this);
  instance = nullptr;
}
