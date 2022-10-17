// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/scoped_password_settings_reauth_module_override.h"

#import "base/check.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
raw_ptr<ScopedPasswordSettingsReauthModuleOverride>
    ScopedPasswordSettingsReauthModuleOverride::instance;

// static
std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
ScopedPasswordSettingsReauthModuleOverride::MakeAndArmForTesting(
    id<ReauthenticationProtocol> module) {
  DCHECK(!instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride> new_instance(
      new ScopedPasswordSettingsReauthModuleOverride);
  new_instance->module = module;
  instance = new_instance.get();
  return new_instance;
}

ScopedPasswordSettingsReauthModuleOverride::
    ~ScopedPasswordSettingsReauthModuleOverride() {
  DCHECK(instance == this);
  instance = nullptr;
}
