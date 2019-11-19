// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_DRIVER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_DRIVER_H_

#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace autofill {
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {
class PasswordAutofillManager;
class PasswordManager;
}  // namespace password_manager

// Defines the interface the driver needs to the controller.
@protocol PasswordManagerDriverDelegate

@property(readonly, nonatomic) const GURL& lastCommittedURL;

- (password_manager::PasswordManager*)passwordManager;

// Finds and fills the password form using the supplied |formData| to
// match the password form and to populate the field values. Calls
// |completionHandler| with YES if a form field has been filled, NO otherwise.
// |completionHandler| can be nil.
- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
       completionHandler:(void (^)(BOOL))completionHandler;

// Informs delegate that there are no saved credentials for the current page.
- (void)onNoSavedCredentials;

// Gets the PasswordGenerationFrameHelper owned by this delegate.
- (password_manager::PasswordGenerationFrameHelper*)passwordGenerationHelper;

// Informs delegate of form for password generation found.
- (void)formEligibleForGenerationFound:
    (const autofill::PasswordFormGenerationData&)form;

@end

// An iOS implementation of password_manager::PasswordManagerDriver.
class IOSChromePasswordManagerDriver
    : public password_manager::PasswordManagerDriver {
 public:
  explicit IOSChromePasswordManagerDriver(
      id<PasswordManagerDriverDelegate> controller);
  ~IOSChromePasswordManagerDriver() override;

  // password_manager::PasswordManagerDriver implementation.
  int GetId() const override;
  void FillPasswordForm(
      const autofill::PasswordFormFillData& form_data) override;
  void InformNoSavedCredentials() override;
  void FormEligibleForGenerationFound(
      const autofill::PasswordFormGenerationData& form) override;
  void GeneratedPasswordAccepted(const base::string16& password) override;
  void FillSuggestion(const base::string16& username,
                      const base::string16& password) override;
  void PreviewSuggestion(const base::string16& username,
                         const base::string16& password) override;
  void ClearPreviewedForm() override;
  password_manager::PasswordGenerationFrameHelper* GetPasswordGenerationHelper()
      override;
  password_manager::PasswordManager* GetPasswordManager() override;
  password_manager::PasswordAutofillManager* GetPasswordAutofillManager()
      override;
  autofill::AutofillDriver* GetAutofillDriver() override;
  bool IsMainFrame() const override;
  const GURL& GetLastCommittedURL() const override;

 private:
  id<PasswordManagerDriverDelegate> delegate_;  // (weak)

  DISALLOW_COPY_AND_ASSIGN(IOSChromePasswordManagerDriver);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_DRIVER_H_
