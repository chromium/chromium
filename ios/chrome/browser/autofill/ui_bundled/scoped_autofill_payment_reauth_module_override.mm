// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/autofill/ui_bundled/scoped_autofill_payment_reauth_module_override.h"

#import "base/check.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

namespace {
ScopedAutofillPaymentReauthModuleOverride* g_instance = nullptr;
}

// static
std::unique_ptr<ScopedAutofillPaymentReauthModuleOverride>
ScopedAutofillPaymentReauthModuleOverride::MakeAndArmForTesting(
    id<ReauthenticationProtocol> module) {
  CHECK(!g_instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedAutofillPaymentReauthModuleOverride> new_instance(
      new ScopedAutofillPaymentReauthModuleOverride);
  new_instance->module = module;
  g_instance = new_instance.get();
  return new_instance;
}

ScopedAutofillPaymentReauthModuleOverride::
    ~ScopedAutofillPaymentReauthModuleOverride() {
  CHECK(g_instance == this);
  g_instance = nullptr;
}

// static
id<ReauthenticationProtocol> ScopedAutofillPaymentReauthModuleOverride::Get() {
  if (g_instance) {
    return g_instance->module;
  }
  return nil;
}
