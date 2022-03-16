// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_SHARING_PERMISSION_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_SHARING_PERMISSION_DELEGATE_H_

#include "content/public/browser/federated_identity_sharing_permission_context_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class MockSharingPermissionDelegate
    : public FederatedIdentitySharingPermissionContextDelegate {
 public:
  MockSharingPermissionDelegate();

  ~MockSharingPermissionDelegate() override;

  MockSharingPermissionDelegate(const MockSharingPermissionDelegate&) = delete;
  MockSharingPermissionDelegate& operator=(
      const MockSharingPermissionDelegate&) = delete;

  MOCK_METHOD(bool,
              HasSharingPermissionForAccount,
              (const url::Origin& identity_provider,
               const url::Origin& relying_party,
               const std::string& account_id),
              (override));
  MOCK_METHOD(void,
              GrantSharingPermissionForAccount,
              (const url::Origin& identity_provider,
               const url::Origin& relying_party,
               const std::string& account_id),
              (override));
  MOCK_METHOD(void,
              RevokeSharingPermissionForAccount,
              (const url::Origin& identity_provider,
               const url::Origin& relying_party,
               const std::string& account_id),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_SHARING_PERMISSION_DELEGATE_H_
