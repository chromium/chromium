// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_DIGITAL_IDENTITY_PROVIDER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_DIGITAL_IDENTITY_PROVIDER_H_

#include "base/values.h"
#include "content/public/browser/digital_identity_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockDigitalIdentityProvider : public DigitalIdentityProvider {
 public:
  MockDigitalIdentityProvider();

  ~MockDigitalIdentityProvider() override;

  MockDigitalIdentityProvider(const MockDigitalIdentityProvider&) = delete;
  MockDigitalIdentityProvider& operator=(const MockDigitalIdentityProvider&) =
      delete;

  MOCK_METHOD4(Request,
               void(WebContents*,
                    const url::Origin& origin,
                    const base::Value::Dict&,
                    DigitalIdentityCallback));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_DIGITAL_IDENTITY_PROVIDER_H_
