// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_UNITTEST_BASE_CHROMEOS_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_UNITTEST_BASE_CHROMEOS_H_

#include <memory>

#include "extensions/browser/api_unittest.h"

namespace extensions {

class ExtensionsAPIClient;

// Creates a FeedbackPrivateDelegate that can generate a SystemLogsSource for
// testing.
class FeedbackPrivateApiUnittestBase : public ApiUnitTest {
 public:
  FeedbackPrivateApiUnittestBase();

  FeedbackPrivateApiUnittestBase(const FeedbackPrivateApiUnittestBase&) =
      delete;
  FeedbackPrivateApiUnittestBase& operator=(
      const FeedbackPrivateApiUnittestBase&) = delete;

  ~FeedbackPrivateApiUnittestBase() override;

  void SetUp() override;
  void TearDown() override;

 private:
  std::unique_ptr<ExtensionsAPIClient> extensions_api_client_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_UNITTEST_BASE_CHROMEOS_H_
