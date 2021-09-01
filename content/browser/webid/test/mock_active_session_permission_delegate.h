// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_ACTIVE_SESSION_PERMISSION_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_ACTIVE_SESSION_PERMISSION_DELEGATE_H_

#include "content/public/browser/federated_identity_active_session_permission_context_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockActiveSessionPermissionDelegate
    : public FederatedIdentityActiveSessionPermissionContextDelegate {
 public:
  MockActiveSessionPermissionDelegate();

  ~MockActiveSessionPermissionDelegate() override;

  MockActiveSessionPermissionDelegate(
      const MockActiveSessionPermissionDelegate&) = delete;
  MockActiveSessionPermissionDelegate& operator=(
      const MockActiveSessionPermissionDelegate&) = delete;

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
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_ACTIVE_SESSION_PERMISSION_DELEGATE_H_
