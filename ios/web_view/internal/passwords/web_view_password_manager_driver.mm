// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_manager_driver.h"

#include <string>

#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordAutofillManager;
using password_manager::PasswordManager;

namespace ios_web_view {

WebViewPasswordManagerDriver::WebViewPasswordManagerDriver(
    password_manager::PasswordManager* password_manager)
    : password_manager_(password_manager) {}

WebViewPasswordManagerDriver::~WebViewPasswordManagerDriver() = default;

int WebViewPasswordManagerDriver::GetId() const {
  // There is only one driver per tab on iOS so returning 0 is fine.
  return 0;
}

void WebViewPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
  [bridge_ fillPasswordForm:form_data completionHandler:nil];
}

void WebViewPasswordManagerDriver::InformNoSavedCredentials(
    bool should_show_popup_without_passwords) {
  [bridge_ onNoSavedCredentials];
}

void WebViewPasswordManagerDriver::FormEligibleForGenerationFound(
    const autofill::PasswordFormGenerationData& form) {
  if (GetPasswordGenerationHelper() &&
      GetPasswordGenerationHelper()->IsGenerationEnabled(
          /*log_debug_data*/ true)) {
    [bridge_ formEligibleForGenerationFound:form];
  }
}

void WebViewPasswordManagerDriver::GeneratedPasswordAccepted(
    const std::u16string& password) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerDriver::FillSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerDriver::PreviewSuggestion(
    const std::u16string& username,
    const std::u16string& password) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerDriver::ClearPreviewedForm() {
  NOTIMPLEMENTED();
}

password_manager::PasswordGenerationFrameHelper*
WebViewPasswordManagerDriver::GetPasswordGenerationHelper() {
  return [bridge_ passwordGenerationHelper];
}

PasswordManager* WebViewPasswordManagerDriver::GetPasswordManager() {
  return password_manager_;
}

PasswordAutofillManager*
WebViewPasswordManagerDriver::GetPasswordAutofillManager() {
  return nullptr;
}

bool WebViewPasswordManagerDriver::IsInPrimaryMainFrame() const {
  // On IOS only processing of password forms in main frame is implemented.
  return true;
}

bool WebViewPasswordManagerDriver::CanShowAutofillUi() const {
  return true;
}

::ui::AXTreeID WebViewPasswordManagerDriver::GetAxTreeId() const {
  return {};
}

const GURL& WebViewPasswordManagerDriver::GetLastCommittedURL() const {
  return bridge_.lastCommittedURL;
}
}  // namespace ios_web_view
