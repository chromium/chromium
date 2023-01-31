// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_AUTO_SIGNIN_PERMISSION_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_AUTO_SIGNIN_PERMISSION_DELEGATE_H_

#include "content/public/browser/federated_identity_auto_signin_permission_context_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockAutoSigninPermissionDelegate
    : public FederatedIdentityAutoSigninPermissionContextDelegate {
 public:
  MockAutoSigninPermissionDelegate();

  ~MockAutoSigninPermissionDelegate() override;

  MockAutoSigninPermissionDelegate(const MockAutoSigninPermissionDelegate&) =
      delete;
  MockAutoSigninPermissionDelegate& operator=(
      const MockAutoSigninPermissionDelegate&) = delete;

  MOCK_METHOD0(HasAutoSigninPermission, bool());
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_AUTO_SIGNIN_PERMISSION_DELEGATE_H_
