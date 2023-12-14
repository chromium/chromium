// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_DM_TOKEN_STORAGE_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_DM_TOKEN_STORAGE_IOS_H_

#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"

namespace policy {

// Implementation of BrowserDMTokenStorage delegate for iOS.
class BrowserDMTokenStorageIOS : public BrowserDMTokenStorage::Delegate {
 public:
  BrowserDMTokenStorageIOS();
  BrowserDMTokenStorageIOS(const BrowserDMTokenStorageIOS&) = delete;
  BrowserDMTokenStorageIOS& operator=(const BrowserDMTokenStorageIOS&) = delete;
  ~BrowserDMTokenStorageIOS() override;

 private:
  // BrowserDMTokenStorage::Delegate implementation.
  std::string InitClientId() override;
  std::string InitEnrollmentToken() override;
  std::string InitDMToken() override;
  bool InitEnrollmentErrorOption() override;
  bool CanInitEnrollmentToken() const override;
  BrowserDMTokenStorage::StoreTask SaveDMTokenTask(
      const std::string& token,
      const std::string& client_id) override;
  BrowserDMTokenStorage::StoreTask DeleteDMTokenTask(
      const std::string& client_id) override;
  scoped_refptr<base::TaskRunner> SaveDMTokenTaskRunner() override;

  scoped_refptr<base::TaskRunner> task_runner_;

  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageIOSTest, InitClientId);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageIOSTest, InitEnrollmentToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageIOSTest, StoreAndLoadDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageIOSTest, DeleteDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageIOSTest, DeleteEmptyDMToken);
  FRIEND_TEST_ALL_PREFIXES(BrowserDMTokenStorageIOSTest,
                           InitDMTokenWithoutDirectory);
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_DM_TOKEN_STORAGE_IOS_H_
