// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_password_manager_driver.h"

#include "base/strings/string16.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordAutofillManager;
using password_manager::PasswordManager;

namespace ios_web_view {
WebViewPasswordManagerDriver::WebViewPasswordManagerDriver(
    id<CWVPasswordManagerDriverDelegate> delegate)
    : delegate_(delegate) {}

WebViewPasswordManagerDriver::~WebViewPasswordManagerDriver() = default;

int WebViewPasswordManagerDriver::GetId() const {
  // There is only one driver per tab on iOS so returning 0 is fine.
  return 0;
}

void WebViewPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
  [delegate_ fillPasswordForm:form_data];
}

void WebViewPasswordManagerDriver::InformNoSavedCredentials() {
  [delegate_ informNoSavedCredentials];
}

void WebViewPasswordManagerDriver::GeneratedPasswordAccepted(
    const base::string16& password) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerDriver::FillSuggestion(
    const base::string16& username,
    const base::string16& password) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerDriver::PreviewSuggestion(
    const base::string16& username,
    const base::string16& password) {
  NOTIMPLEMENTED();
}

void WebViewPasswordManagerDriver::ClearPreviewedForm() {
  NOTIMPLEMENTED();
}

password_manager::PasswordGenerationFrameHelper*
WebViewPasswordManagerDriver::GetPasswordGenerationHelper() {
  return nullptr;
}

PasswordManager* WebViewPasswordManagerDriver::GetPasswordManager() {
  return [delegate_ passwordManager];
}

PasswordAutofillManager*
WebViewPasswordManagerDriver::GetPasswordAutofillManager() {
  return nullptr;
}

autofill::AutofillDriver* WebViewPasswordManagerDriver::GetAutofillDriver() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool WebViewPasswordManagerDriver::IsMainFrame() const {
  // On IOS only processing of password forms in main frame is implemented.
  return true;
}

const GURL& WebViewPasswordManagerDriver::GetLastCommittedURL() const {
  return delegate_.lastCommittedURL;
}
}  // namespace ios_web_view
