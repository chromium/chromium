// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_SCOPED_FORM_INPUT_ACCESSORY_REAUTH_MODULE_OVERRIDE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_SCOPED_FORM_INPUT_ACCESSORY_REAUTH_MODULE_OVERRIDE_H_

#import <memory>

@protocol ReauthenticationProtocol;

// Util class enabling a global override of the Reauthentication Module used in
// newly-constructed FormInputAccessoryCoordinator, for testing purposes only.
// Only one ScopedFormInputAccessoryReauthModuleOverride scope should be used at
// any given time, these scopes can't be nested.
class ScopedFormInputAccessoryReauthModuleOverride {
 public:
  ~ScopedFormInputAccessoryReauthModuleOverride();

  // Returns the override module, if one exists.
  static id<ReauthenticationProtocol> Get();

  // Creates a scoped override so that the provided fake/mock/disarmed/etc
  // reauthentication module will be used in place of the production
  // implementation.
  // FormInputAccessoryMediator objects will use `module` as their
  // reauthentication module until the override is destroyed.
  static std::unique_ptr<ScopedFormInputAccessoryReauthModuleOverride>
  MakeAndArmForTesting(id<ReauthenticationProtocol> module);

  // The module to be used.
  id<ReauthenticationProtocol> module;

 private:
  ScopedFormInputAccessoryReauthModuleOverride() = default;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_SCOPED_FORM_INPUT_ACCESSORY_REAUTH_MODULE_OVERRIDE_H_
