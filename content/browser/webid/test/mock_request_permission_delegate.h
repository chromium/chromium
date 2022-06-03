// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_REQUEST_PERMISSION_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_REQUEST_PERMISSION_DELEGATE_H_

#include "content/public/browser/federated_identity_request_permission_context_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockRequestPermissionDelegate
    : public FederatedIdentityRequestPermissionContextDelegate {
 public:
  MockRequestPermissionDelegate();

  ~MockRequestPermissionDelegate() override;

  MockRequestPermissionDelegate(const MockRequestPermissionDelegate&) = delete;
  MockRequestPermissionDelegate& operator=(
      const MockRequestPermissionDelegate&) = delete;

  MOCK_METHOD2(HasRequestPermission,
               bool(const url::Origin&, const url::Origin&));
  MOCK_METHOD2(GrantRequestPermission,
               void(const url::Origin&, const url::Origin&));
  MOCK_METHOD2(RevokeRequestPermission,
               void(const url::Origin&, const url::Origin&));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_REQUEST_PERMISSION_DELEGATE_H_
