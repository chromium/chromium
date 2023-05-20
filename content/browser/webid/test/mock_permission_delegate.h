// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_PERMISSION_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_PERMISSION_DELEGATE_H_

#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace content {

class MockPermissionDelegate
    : public FederatedIdentityPermissionContextDelegate {
 public:
  MockPermissionDelegate();

  ~MockPermissionDelegate() override;

  MockPermissionDelegate(const MockPermissionDelegate&) = delete;
  MockPermissionDelegate& operator=(const MockPermissionDelegate&) = delete;

  MOCK_METHOD1(AddIdpSigninStatusObserver, void(IdpSigninStatusObserver*));
  MOCK_METHOD1(RemoveIdpSigninStatusObserver, void(IdpSigninStatusObserver*));
  MOCK_METHOD3(HasActiveSession,
               bool(const url::Origin&,
                    const url::Origin&,
                    const std::string&));
  MOCK_METHOD3(GrantActiveSession,
               void(const url::Origin&,
                    const url::Origin&,
                    const std::string&));
  MOCK_METHOD3(RevokeActiveSession,
               void(const url::Origin&,
                    const url::Origin&,
                    const std::string&));
  MOCK_METHOD4(HasSharingPermission,
               bool(const url::Origin&,
                    const url::Origin&,
                    const url::Origin& identity_provider,
                    const absl::optional<std::string>& account_id));
  MOCK_METHOD1(HasSharingPermission, bool(const url::Origin&));
  MOCK_METHOD4(GrantSharingPermission,
               void(const url::Origin&,
                    const url::Origin&,
                    const url::Origin&,
                    const std::string&));
  MOCK_METHOD1(GetIdpSigninStatus, absl::optional<bool>(const url::Origin&));
  MOCK_METHOD2(SetIdpSigninStatus, void(const url::Origin&, bool));
  MOCK_METHOD1(RegisterIdP, void(const ::GURL&));
  MOCK_METHOD1(UnregisterIdP, void(const ::GURL&));
  MOCK_METHOD0(GetRegisteredIdPs, std::vector<GURL>());
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_PERMISSION_DELEGATE_H_
