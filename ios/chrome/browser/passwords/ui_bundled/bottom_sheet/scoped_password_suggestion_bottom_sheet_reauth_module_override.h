// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_SCOPED_PASSWORD_SUGGESTION_BOTTOM_SHEET_REAUTH_MODULE_OVERRIDE_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_SCOPED_PASSWORD_SUGGESTION_BOTTOM_SHEET_REAUTH_MODULE_OVERRIDE_H_

#import <memory>

@protocol ReauthenticationProtocol;

// Util class enabling a global override of the Reauthentication Module used in
// newly-constructed PasswordSuggestionBottomSheetCoordinator, for testing
// purposes only.
class ScopedPasswordSuggestionBottomSheetReauthModuleOverride {
 public:
  ~ScopedPasswordSuggestionBottomSheetReauthModuleOverride();

  // Returns the override module, if one exists.
  static id<ReauthenticationProtocol> Get();

  // Creates a scoped override so that the provided fake/mock/disarmed/etc
  // reauthentication module will be used in place of the production
  // implementation.
  // Newly created coordinators will use `module` as their reauthentication
  // module until the override is destroyed. Any coordinator created while an
  // override is active will hold a strong ref to `module`.
  static std::unique_ptr<
      ScopedPasswordSuggestionBottomSheetReauthModuleOverride>
  MakeAndArmForTesting(id<ReauthenticationProtocol> module);

  // The module to be used.
  id<ReauthenticationProtocol> module;

 private:
  ScopedPasswordSuggestionBottomSheetReauthModuleOverride() = default;
};

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_SCOPED_PASSWORD_SUGGESTION_BOTTOM_SHEET_REAUTH_MODULE_OVERRIDE_H_
