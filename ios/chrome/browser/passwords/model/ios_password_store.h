// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_STORE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_STORE_H_

#include <memory>

#include "components/password_manager/core/browser/password_store/password_store.h"

class ProfileIOS;

namespace password_manager {

class StoreMetricsReporter;

// Same as `PasswordStore`.
// Furthermore, for regular profile, query the password stores and reports
// multiple metrics. The actual reporting is delayed by 30 seconds, to ensure it
// doesn't happen during the "hot phase" of Chrome startup.
class PasswordStoreIOS : public PasswordStore {
 public:
  explicit PasswordStoreIOS(std::unique_ptr<PasswordStoreBackend> backend,
                            ProfileIOS* profile);
  PasswordStoreIOS(const PasswordStore&) = delete;
  PasswordStoreIOS& operator=(const PasswordStore&) = delete;
  void ShutdownOnUIThread() override;

 private:
  ~PasswordStoreIOS() override;
  void StartMetricsReporting();
  void FreeMetricsReporter();

  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<password_manager::StoreMetricsReporter> metrics_reporter_;
  base::WeakPtrFactory<PasswordStoreIOS> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_PASSWORD_STORE_H_
