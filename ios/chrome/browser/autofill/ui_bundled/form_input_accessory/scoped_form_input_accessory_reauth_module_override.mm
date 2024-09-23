// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/scoped_form_input_accessory_reauth_module_override.h"

#import "base/check.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

namespace {
ScopedFormInputAccessoryReauthModuleOverride* g_instance = nullptr;
}

// static
std::unique_ptr<ScopedFormInputAccessoryReauthModuleOverride>
ScopedFormInputAccessoryReauthModuleOverride::MakeAndArmForTesting(
    id<ReauthenticationProtocol> module) {
  DCHECK(!g_instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedFormInputAccessoryReauthModuleOverride> new_instance(
      new ScopedFormInputAccessoryReauthModuleOverride);
  new_instance->module = module;
  g_instance = new_instance.get();
  return new_instance;
}

ScopedFormInputAccessoryReauthModuleOverride::
    ~ScopedFormInputAccessoryReauthModuleOverride() {
  DCHECK(g_instance == this);
  g_instance = nullptr;
}

// static
id<ReauthenticationProtocol>
ScopedFormInputAccessoryReauthModuleOverride::Get() {
  if (g_instance) {
    return g_instance->module;
  }
  return nil;
}
