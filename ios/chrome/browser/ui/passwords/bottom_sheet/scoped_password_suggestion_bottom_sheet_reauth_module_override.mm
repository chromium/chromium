// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/scoped_password_suggestion_bottom_sheet_reauth_module_override.h"

#import "base/check.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

// static
raw_ptr<ScopedPasswordSuggestionBottomSheetReauthModuleOverride>
    ScopedPasswordSuggestionBottomSheetReauthModuleOverride::instance;

// static
std::unique_ptr<ScopedPasswordSuggestionBottomSheetReauthModuleOverride>
ScopedPasswordSuggestionBottomSheetReauthModuleOverride::MakeAndArmForTesting(
    id<ReauthenticationProtocol> module) {
  DCHECK(!instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedPasswordSuggestionBottomSheetReauthModuleOverride>
      new_instance(new ScopedPasswordSuggestionBottomSheetReauthModuleOverride);
  new_instance->module = module;
  instance = new_instance.get();
  return new_instance;
}

ScopedPasswordSuggestionBottomSheetReauthModuleOverride::
    ~ScopedPasswordSuggestionBottomSheetReauthModuleOverride() {
  DCHECK(instance == this);
  instance = nullptr;
}
