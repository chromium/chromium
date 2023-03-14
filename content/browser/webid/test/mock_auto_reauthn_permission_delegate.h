// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_AUTO_REAUTHN_PERMISSION_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_AUTO_REAUTHN_PERMISSION_DELEGATE_H_

#include "content/public/browser/federated_identity_auto_reauthn_permission_context_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockAutoReauthnPermissionDelegate
    : public FederatedIdentityAutoReauthnPermissionContextDelegate {
 public:
  MockAutoReauthnPermissionDelegate();

  ~MockAutoReauthnPermissionDelegate() override;

  MockAutoReauthnPermissionDelegate(const MockAutoReauthnPermissionDelegate&) =
      delete;
  MockAutoReauthnPermissionDelegate& operator=(
      const MockAutoReauthnPermissionDelegate&) = delete;

  MOCK_METHOD0(HasAutoReauthnContentSetting, bool());
  MOCK_METHOD1(IsAutoReauthnEmbargoed, bool(const url::Origin&));
  MOCK_METHOD1(GetAutoReauthnEmbargoStartTime, base::Time(const url::Origin&));
  MOCK_METHOD1(RecordDisplayAndEmbargo, void(const url::Origin&));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_AUTO_REAUTHN_PERMISSION_DELEGATE_H_
