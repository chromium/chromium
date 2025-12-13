// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/scoped_credential_suggestion_bottom_sheet_reauth_module_override.h"

#import "base/check.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

namespace {
ScopedCredentialSuggestionBottomSheetReauthModuleOverride* g_instance = nullptr;
}

// static
std::unique_ptr<ScopedCredentialSuggestionBottomSheetReauthModuleOverride>
ScopedCredentialSuggestionBottomSheetReauthModuleOverride::MakeAndArmForTesting(
    id<ReauthenticationProtocol> module) {
  DCHECK(!g_instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedCredentialSuggestionBottomSheetReauthModuleOverride>
      new_instance(
          new ScopedCredentialSuggestionBottomSheetReauthModuleOverride);
  new_instance->module = module;
  g_instance = new_instance.get();
  return new_instance;
}

ScopedCredentialSuggestionBottomSheetReauthModuleOverride::
    ~ScopedCredentialSuggestionBottomSheetReauthModuleOverride() {
  DCHECK(g_instance == this);
  g_instance = nullptr;
}

id<ReauthenticationProtocol>
ScopedCredentialSuggestionBottomSheetReauthModuleOverride::Get() {
  if (g_instance) {
    return g_instance->module;
  }
  return nil;
}
