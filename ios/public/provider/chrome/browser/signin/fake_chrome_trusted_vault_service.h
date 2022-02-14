// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_TRUSTED_VAULT_SERVICE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_TRUSTED_VAULT_SERVICE_H_

#import "ios/public/provider/chrome/browser/signin/chrome_trusted_vault_service.h"

@class FakeTrustedVaultViewController;

namespace ios {

// A fake ChromeTrustedVaultService used for testing.
class FakeChromeTrustedVaultService : public ChromeTrustedVaultService {
 public:
  FakeChromeTrustedVaultService();
  ~FakeChromeTrustedVaultService() override;

  // Convenience method that returns the instance of
  // |FakeChromeIdentityService| from the ChromeBrowserProvider.
  static FakeChromeTrustedVaultService* GetInstanceFromChromeProvider();

  void FetchKeys(ChromeIdentity* chrome_identity,
                 base::OnceCallback<void(const TrustedVaultSharedKeyList&)>
                     callback) override;
  void MarkLocalKeysAsStale(ChromeIdentity* chrome_identity,
                            base::OnceClosure callback) override;
  void GetDegradedRecoverabilityStatus(
      ChromeIdentity* chrome_identity,
      base::OnceCallback<void(bool)> callback) override;
  void FixDegradedRecoverability(ChromeIdentity* chrome_identity,
                                 UIViewController* presentingViewController,
                                 void (^callback)(BOOL success,
                                                  NSError* error)) override;
  void Reauthentication(ChromeIdentity* chrome_identity,
                        UIViewController* presenting_view_controller,
                        void (^callback)(BOOL success,
                                         NSError* error)) override;
  void CancelDialog(BOOL animated, void (^callback)(void)) override;
  void ClearLocalDataForIdentity(ChromeIdentity* chrome_identity,
                                 void (^callback)(BOOL success,
                                                  NSError* error)) override {}

  // Simulates user cancel the reauth dialog.
  void SimulateUserCancel();

 protected:
  FakeTrustedVaultViewController* trusted_vault_view_controller_;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_TRUSTED_VAULT_SERVICE_H_
