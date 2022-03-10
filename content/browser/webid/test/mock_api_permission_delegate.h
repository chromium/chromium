// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_API_PERMISSION_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_API_PERMISSION_DELEGATE_H_

#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockApiPermissionDelegate
    : public FederatedIdentityApiPermissionContextDelegate {
 public:
  MockApiPermissionDelegate();

  ~MockApiPermissionDelegate() override;

  MockApiPermissionDelegate(const MockApiPermissionDelegate&) = delete;
  MockApiPermissionDelegate& operator=(const MockApiPermissionDelegate&) =
      delete;

  MOCK_METHOD(bool, HasApiPermission, (), (override));
  MOCK_METHOD(bool, AreThirdPartyCookiesBlocked, (), (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_API_PERMISSION_DELEGATE_H_
