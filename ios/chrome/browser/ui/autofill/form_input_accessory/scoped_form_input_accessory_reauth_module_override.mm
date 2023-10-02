// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/scoped_form_input_accessory_reauth_module_override.h"

#import "base/check.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

// static
raw_ptr<ScopedFormInputAccessoryReauthModuleOverride>
    ScopedFormInputAccessoryReauthModuleOverride::instance;

// static
std::unique_ptr<ScopedFormInputAccessoryReauthModuleOverride>
ScopedFormInputAccessoryReauthModuleOverride::MakeAndArmForTesting(
    id<ReauthenticationProtocol> module) {
  DCHECK(!instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedFormInputAccessoryReauthModuleOverride> new_instance(
      new ScopedFormInputAccessoryReauthModuleOverride);
  new_instance->module = module;
  instance = new_instance.get();
  return new_instance;
}

ScopedFormInputAccessoryReauthModuleOverride::
    ~ScopedFormInputAccessoryReauthModuleOverride() {
  DCHECK(instance == this);
  instance = nullptr;
}
