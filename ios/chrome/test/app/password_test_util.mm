// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/password_test_util.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

// Replace the reauthentication module in
// PasswordDetailsCollectionViewController with a fake one to avoid being
// blocked with a reauth prompt, and return the fake reauthentication module.
MockReauthenticationModule* SetUpAndReturnMockReauthenticationModule(
    bool is_add_new_password) {
  MockReauthenticationModule* mock_reauthentication_module =
      [[MockReauthenticationModule alloc] init];
  // TODO(crbug.com/754642): Stop using TopPresentedViewController();
  UINavigationController* ui_navigation_controller =
      base::mac::ObjCCastStrict<UINavigationController>(
          top_view_controller::TopPresentedViewController());
  if (is_add_new_password) {
    AddPasswordViewController* add_password_view_controller =
        base::mac::ObjCCastStrict<AddPasswordViewController>(
            ui_navigation_controller.topViewController);
    add_password_view_controller.reauthModule = mock_reauthentication_module;
  } else {
    PasswordDetailsTableViewController* password_details_table_view_controller =
        base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
            ui_navigation_controller.topViewController);
    password_details_table_view_controller.reauthModule =
        mock_reauthentication_module;
  }
  return mock_reauthentication_module;
}

// Replaces the reauthentication module in Password Manager's passwords list
// with a fake one to avoid being blocked with a reauth prompt and returns the
// fake reauthentication module.
MockReauthenticationModule*
SetUpAndReturnMockReauthenticationModuleForPasswordManager() {
  MockReauthenticationModule* mock_reauthentication_module =
      [[MockReauthenticationModule alloc] init];
  // TODO(crbug.com/754642): Stop using TopPresentedViewController();
  SettingsNavigationController* settings_navigation_controller =
      base::mac::ObjCCastStrict<SettingsNavigationController>(
          top_view_controller::TopPresentedViewController());
  PasswordManagerViewController* password_manager_view_controller =
      base::mac::ObjCCastStrict<PasswordManagerViewController>(
          settings_navigation_controller.topViewController);
  password_manager_view_controller.reauthenticationModule =
      mock_reauthentication_module;
  return mock_reauthentication_module;
}

// Replace the reauthentication module in Password Settings'
// PasswordExporter with a fake one to avoid being
// blocked with a reauth prompt, and return the fake reauthentication module.
std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
SetUpAndReturnMockReauthenticationModuleForExportFromSettings() {
  MockReauthenticationModule* mock_reauthentication_module =
      [[MockReauthenticationModule alloc] init];
  return ScopedPasswordSettingsReauthModuleOverride::MakeAndArmForTesting(
      mock_reauthentication_module);
}

std::unique_ptr<ScopedPasswordSuggestionBottomSheetReauthModuleOverride>
SetUpAndReturnMockReauthenticationModuleForPasswordSuggestionBottomSheet() {
  MockReauthenticationModule* mock_reauthentication_module =
      [[MockReauthenticationModule alloc] init];
  return ScopedPasswordSuggestionBottomSheetReauthModuleOverride::
      MakeAndArmForTesting(mock_reauthentication_module);
}

}  // namespace
