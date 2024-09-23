// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_PERMISSION_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_PERMISSION_DELEGATE_H_

#include "base/functional/callback.h"
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

  MOCK_METHOD(void,
              AddIdpSigninStatusObserver,
              (IdpSigninStatusObserver*),
              (override));
  MOCK_METHOD(void,
              RemoveIdpSigninStatusObserver,
              (IdpSigninStatusObserver*),
              (override));
  MOCK_METHOD(bool,
              HasSharingPermission,
              (const url::Origin&,
               const url::Origin&,
               const url::Origin& identity_provider),
              (override));
  MOCK_METHOD(std::optional<base::Time>,
              GetLastUsedTimestamp,
              (const url::Origin& relying_party_requester,
               const url::Origin& relying_party_embedder,
               const url::Origin& identity_provider,
               const std::string& account_id),
              (override));
  MOCK_METHOD(bool, HasSharingPermission, (const url::Origin&), (override));
  MOCK_METHOD(void,
              GrantSharingPermission,
              (const url::Origin&,
               const url::Origin&,
               const url::Origin&,
               const std::string&),
              (override));
  MOCK_METHOD(void,
              RevokeSharingPermission,
              (const url::Origin&,
               const url::Origin&,
               const url::Origin&,
               const std::string&),
              (override));
  MOCK_METHOD(void,
              RefreshExistingSharingPermission,
              (const url::Origin&,
               const url::Origin&,
               const url::Origin&,
               const std::string&),
              (override));
  MOCK_METHOD(std::optional<bool>,
              GetIdpSigninStatus,
              (const url::Origin&),
              (override));
  MOCK_METHOD(void, SetIdpSigninStatus, (const url::Origin&, bool), (override));
  MOCK_METHOD(void, RegisterIdP, (const ::GURL&), (override));
  MOCK_METHOD(void, UnregisterIdP, (const ::GURL&), (override));
  MOCK_METHOD(std::vector<GURL>, GetRegisteredIdPs, (), (override));
  MOCK_METHOD(void,
              OnSetRequiresUserMediation,
              (const url::Origin&, base::OnceClosure),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_PERMISSION_DELEGATE_H_
