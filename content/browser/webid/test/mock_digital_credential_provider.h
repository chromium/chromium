// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_DIGITAL_CREDENTIAL_PROVIDER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_DIGITAL_CREDENTIAL_PROVIDER_H_

#include "base/values.h"
#include "content/browser/webid/digital_credentials/digital_credential_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockDigitalCredentialProvider : public DigitalCredentialProvider {
 public:
  MockDigitalCredentialProvider();

  ~MockDigitalCredentialProvider() override;

  MockDigitalCredentialProvider(const MockDigitalCredentialProvider&) = delete;
  MockDigitalCredentialProvider& operator=(
      const MockDigitalCredentialProvider&) = delete;

  MOCK_METHOD4(RequestDigitalCredential,
               void(WebContents*,
                    const url::Origin& origin,
                    const base::Value::Dict&,
                    DigitalCredentialCallback));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_DIGITAL_CREDENTIAL_PROVIDER_H_
