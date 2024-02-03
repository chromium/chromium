// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REGISTRY_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REGISTRY_H_

#include "content/browser/webid/identity_registry.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

namespace content {

class MockIdentityRegistry : public IdentityRegistry {
 public:
  explicit MockIdentityRegistry(
      content::WebContents* web_contents,
      base::WeakPtr<FederatedIdentityModalDialogViewDelegate> delegate,
      const GURL& idp_config_url);

  ~MockIdentityRegistry() override;

  MockIdentityRegistry(const MockIdentityRegistry&) = delete;
  MockIdentityRegistry& operator=(const MockIdentityRegistry&) = delete;

  MOCK_METHOD1(Notify, void(const url::Origin&));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_IDENTITY_REGISTRY_H_
