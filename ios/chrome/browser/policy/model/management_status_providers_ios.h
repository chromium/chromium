// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_STATUS_PROVIDERS_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_STATUS_PROVIDERS_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/policy/core/common/management/management_service.h"

class ProfileIOS;

// TODO (crbug.com/1238355): Add unit tests for this file.

namespace policy {

class BrowserCloudManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  BrowserCloudManagementStatusProvider();
  ~BrowserCloudManagementStatusProvider() final;

 protected:
  // ManagementStatusProvider:
  policy::EnterpriseManagementAuthority FetchAuthority() final;
};

class LocalBrowserManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  LocalBrowserManagementStatusProvider();
  ~LocalBrowserManagementStatusProvider() final;

 protected:
  // ManagementStatusProvider:
  policy::EnterpriseManagementAuthority FetchAuthority() final;
};

class LocalDomainBrowserManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  LocalDomainBrowserManagementStatusProvider();
  ~LocalDomainBrowserManagementStatusProvider() final;

 protected:
  // ManagementStatusProvider:
  policy::EnterpriseManagementAuthority FetchAuthority() final;
};

class ProfileCloudManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  explicit ProfileCloudManagementStatusProvider(ProfileIOS* profile);
  ~ProfileCloudManagementStatusProvider() final;

 protected:
  // ManagementStatusProvider:
  policy::EnterpriseManagementAuthority FetchAuthority() final;

 private:
  raw_ptr<ProfileIOS> profile_;
};

class LocalTestPolicyUserManagementProvider final
    : public policy::ManagementStatusProvider {
 public:
  explicit LocalTestPolicyUserManagementProvider(ProfileIOS* profile);
  ~LocalTestPolicyUserManagementProvider() final;

 protected:
  // ManagementStatusProvider:
  policy::EnterpriseManagementAuthority FetchAuthority() final;

 private:
  raw_ptr<ProfileIOS> profile_;
};

class LocalTestPolicyBrowserManagementProvider final
    : public policy::ManagementStatusProvider {
 public:
  explicit LocalTestPolicyBrowserManagementProvider(ProfileIOS* profile);
  ~LocalTestPolicyBrowserManagementProvider() final;

 protected:
  // ManagementStatusProvider:
  policy::EnterpriseManagementAuthority FetchAuthority() final;

 private:
  raw_ptr<ProfileIOS> profile_;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_MANAGEMENT_STATUS_PROVIDERS_IOS_H_
