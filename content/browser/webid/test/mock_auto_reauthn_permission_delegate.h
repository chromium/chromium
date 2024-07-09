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

  MOCK_METHOD(bool, IsAutoReauthnSettingEnabled, (), (override));
  MOCK_METHOD(bool, IsAutoReauthnEmbargoed, (const url::Origin&), (override));
  MOCK_METHOD(base::Time,
              GetAutoReauthnEmbargoStartTime,
              (const url::Origin&),
              (override));
  MOCK_METHOD(void,
              RecordEmbargoForAutoReauthn,
              (const url::Origin&),
              (override));
  MOCK_METHOD(void,
              RemoveEmbargoForAutoReauthn,
              (const url::Origin&),
              (override));
  MOCK_METHOD(void,
              SetRequiresUserMediation,
              (const url::Origin&, bool),
              (override));
  MOCK_METHOD(bool, RequiresUserMediation, (const url::Origin&), (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_AUTO_REAUTHN_PERMISSION_DELEGATE_H_
