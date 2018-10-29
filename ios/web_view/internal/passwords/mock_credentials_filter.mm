// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/passwords/mock_credentials_filter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

MockCredentialsFilter::MockCredentialsFilter() = default;

MockCredentialsFilter::~MockCredentialsFilter() = default;

std::vector<std::unique_ptr<autofill::PasswordForm>>
MockCredentialsFilter::FilterResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) const {
  FilterResultsPtr(&results);
  return results;
}

bool MockCredentialsFilter::ShouldSave(
    const autofill::PasswordForm& form) const {
  return true;
}

bool MockCredentialsFilter::ShouldSaveGaiaPasswordHash(
    const autofill::PasswordForm& form) const {
  return false;
}

bool MockCredentialsFilter::ShouldSaveEnterprisePasswordHash(
    const autofill::PasswordForm& form) const {
  return false;
}

void MockCredentialsFilter::ReportFormLoginSuccess(
    const password_manager::PasswordFormManagerInterface& form_manager) const {}

bool MockCredentialsFilter::IsSyncAccountEmail(
    const std::string& username) const {
  return false;
}

void MockCredentialsFilter::FilterResultsPtr(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* results) const {}

}  // namespace ios_web_view
