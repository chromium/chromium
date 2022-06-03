// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_manager_driver.h"

#include <string>

#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordAutofillManager;
using password_manager::PasswordManager;

IOSChromePasswordManagerDriver::IOSChromePasswordManagerDriver(
    id<PasswordManagerDriverBridge> bridge,
    password_manager::PasswordManager* password_manager)
    : bridge_(bridge), password_manager_(password_manager) {}

IOSChromePasswordManagerDriver::~IOSChromePasswordManagerDriver() = default;

int IOSChromePasswordManagerDriver::GetId() const {
  // There is only one driver per tab on iOS so returning 0 is fine.
  return 0;
}

void IOSChromePasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
  [bridge_ fillPasswordForm:form_data completionHandler:nil];
}

void IOSChromePasswordManagerDriver::InformNoSavedCredentials(
    bool should_show_popup_without_passwords) {
  [bridge_ onNoSavedCredentials];
}

void IOSChromePasswordManagerDriver::FormEligibleForGenerationFound(
    const autofill::PasswordFormGenerationData& form) {
  if (GetPasswordGenerationHelper() &&
      GetPasswordGenerationHelper()->IsGenerationEnabled(
          /*log_debug_data*/ true)) {
    [bridge_ formEligibleForGenerationFound:form];
  }
}

void IOSChromePasswordManagerDriver::GeneratedPasswordAccepted(
    const std::u16string& password) {
  NOTIMPLEMENTED();
}

void IOSChromePasswordManagerDriver::FillSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  NOTIMPLEMENTED();
}

void IOSChromePasswordManagerDriver::PreviewSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  NOTIMPLEMENTED();
}

void IOSChromePasswordManagerDriver::ClearPreviewedForm() {
  NOTIMPLEMENTED();
}

password_manager::PasswordGenerationFrameHelper*
IOSChromePasswordManagerDriver::GetPasswordGenerationHelper() {
  return [bridge_ passwordGenerationHelper];
}

PasswordManager* IOSChromePasswordManagerDriver::GetPasswordManager() {
  return password_manager_;
}

PasswordAutofillManager*
IOSChromePasswordManagerDriver::GetPasswordAutofillManager() {
  // TODO(crbug.com/341877): Use PasswordAutofillManager to implement password
  // autofill on iOS.
  return nullptr;
}

bool IOSChromePasswordManagerDriver::IsInPrimaryMainFrame() const {
  // On IOS only processing of password forms in main frame is implemented.
  return true;
}

bool IOSChromePasswordManagerDriver::CanShowAutofillUi() const {
  return true;
}

::ui::AXTreeID IOSChromePasswordManagerDriver::GetAxTreeId() const {
  return {};
}

const GURL& IOSChromePasswordManagerDriver::GetLastCommittedURL() const {
  return bridge_.lastCommittedURL;
}
