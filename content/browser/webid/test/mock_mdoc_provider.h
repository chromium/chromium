// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_MDOC_PROVIDER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_MDOC_PROVIDER_H_

#include "content/browser/webid/mdocs/mdoc_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockMDocProvider : public MDocProvider {
 public:
  MockMDocProvider();

  ~MockMDocProvider() override;

  MockMDocProvider(const MockMDocProvider&) = delete;
  MockMDocProvider& operator=(const MockMDocProvider&) = delete;

  MOCK_METHOD5(RequestMDoc,
               void(WebContents*,
                    const std::string&,
                    const std::string&,
                    const std::vector<MDocElementPtr>&,
                    MDocCallback));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_MDOC_PROVIDER_H_
