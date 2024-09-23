// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/scoped_password_settings_reauth_module_override.h"

#import "base/check.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

namespace {
ScopedPasswordSettingsReauthModuleOverride* g_instance = nullptr;
}

// static
std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
ScopedPasswordSettingsReauthModuleOverride::MakeAndArmForTesting(
    id<ReauthenticationProtocol> module) {
  DCHECK(!g_instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride> new_instance(
      new ScopedPasswordSettingsReauthModuleOverride);
  new_instance->module = module;
  g_instance = new_instance.get();
  return new_instance;
}

ScopedPasswordSettingsReauthModuleOverride::
    ~ScopedPasswordSettingsReauthModuleOverride() {
  DCHECK(g_instance == this);
  g_instance = nullptr;
}

// static
id<ReauthenticationProtocol> ScopedPasswordSettingsReauthModuleOverride::Get() {
  if (g_instance) {
    return g_instance->module;
  }
  return nil;
}
