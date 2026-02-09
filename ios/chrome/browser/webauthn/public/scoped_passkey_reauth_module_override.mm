// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/public/scoped_passkey_reauth_module_override.h"

#import "base/check_op.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

namespace {
ScopedPasskeyReauthModuleOverride* g_instance = nullptr;
}

// static
std::unique_ptr<ScopedPasskeyReauthModuleOverride>
ScopedPasskeyReauthModuleOverride::MakeAndArmForTesting(  // IN-TEST
    id<ReauthenticationProtocol> module) {
  DCHECK(!g_instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedPasskeyReauthModuleOverride> new_instance(
      new ScopedPasskeyReauthModuleOverride);
  new_instance->module = module;
  g_instance = new_instance.get();
  return new_instance;
}

ScopedPasskeyReauthModuleOverride::~ScopedPasskeyReauthModuleOverride() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

id<ReauthenticationProtocol> ScopedPasskeyReauthModuleOverride::Get() {
  if (g_instance) {
    return g_instance->module;
  }
  return nil;
}
