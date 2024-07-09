// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_API_PERMISSION_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_API_PERMISSION_DELEGATE_H_

#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace url {
class Origin;
}

namespace content {

class MockApiPermissionDelegate
    : public FederatedIdentityApiPermissionContextDelegate {
 public:
  MockApiPermissionDelegate();

  ~MockApiPermissionDelegate() override;

  MockApiPermissionDelegate(const MockApiPermissionDelegate&) = delete;
  MockApiPermissionDelegate& operator=(const MockApiPermissionDelegate&) =
      delete;

  MOCK_METHOD(FederatedIdentityApiPermissionContextDelegate::PermissionStatus,
              GetApiPermissionStatus,
              (const url::Origin&),
              (override));
  MOCK_METHOD(void, RecordDismissAndEmbargo, (const url::Origin&), (override));
  MOCK_METHOD(void,
              RemoveEmbargoAndResetCounts,
              (const url::Origin&),
              (override));
  MOCK_METHOD(bool,
              HasThirdPartyCookiesAccess,
              (RenderFrameHost&, const GURL&, const url::Origin&),
              (const, override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_API_PERMISSION_DELEGATE_H_
