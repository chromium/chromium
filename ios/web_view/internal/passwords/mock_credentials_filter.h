// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_MOCK_CREDENTIALS_FILTER_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_MOCK_CREDENTIALS_FILTER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/credentials_filter.h"

namespace ios_web_view {

// Mock of the CredentialsFilter API, to be used in tests. This filter does
// not filter out anything.
class MockCredentialsFilter : public password_manager::CredentialsFilter {
 public:
  MockCredentialsFilter();

  ~MockCredentialsFilter() override;

  // CredentialsFilter
  std::vector<std::unique_ptr<autofill::PasswordForm>> FilterResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results)
      const override;
  bool ShouldSave(const autofill::PasswordForm& form) const override;
  bool ShouldSaveGaiaPasswordHash(
      const autofill::PasswordForm& form) const override;
  bool ShouldSaveEnterprisePasswordHash(
      const autofill::PasswordForm& form) const override;
  void ReportFormLoginSuccess(
      const password_manager::PasswordFormManagerInterface& form_manager)
      const override;
  bool IsSyncAccountEmail(const std::string& username) const override;

  // A version of FilterResult without moveable arguments, which cannot be
  // mocked in GMock. MockCredentialsFilter::FilterResults(arg) calls
  // FilterResultsPtr(&arg).
  virtual void FilterResultsPtr(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* results) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCredentialsFilter);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_MOCK_CREDENTIALS_FILTER_H_
