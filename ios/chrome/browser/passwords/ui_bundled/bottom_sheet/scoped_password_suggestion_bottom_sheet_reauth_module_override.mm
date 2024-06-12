// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/scoped_password_suggestion_bottom_sheet_reauth_module_override.h"

#import "base/check.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

namespace {
ScopedPasswordSuggestionBottomSheetReauthModuleOverride* g_instance = nullptr;
}

// static
std::unique_ptr<ScopedPasswordSuggestionBottomSheetReauthModuleOverride>
ScopedPasswordSuggestionBottomSheetReauthModuleOverride::MakeAndArmForTesting(
    id<ReauthenticationProtocol> module) {
  DCHECK(!g_instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedPasswordSuggestionBottomSheetReauthModuleOverride>
      new_instance(new ScopedPasswordSuggestionBottomSheetReauthModuleOverride);
  new_instance->module = module;
  g_instance = new_instance.get();
  return new_instance;
}

ScopedPasswordSuggestionBottomSheetReauthModuleOverride::
    ~ScopedPasswordSuggestionBottomSheetReauthModuleOverride() {
  DCHECK(g_instance == this);
  g_instance = nullptr;
}

id<ReauthenticationProtocol>
ScopedPasswordSuggestionBottomSheetReauthModuleOverride::Get() {
  if (g_instance) {
    return g_instance->module;
  }
  return nil;
}
