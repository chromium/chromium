// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

#include "components/variations/net/variations_http_headers.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace ios {
namespace {

using ProfileIOSTest = PlatformTest;

// Tests that ProfileIOS implements UpdateCorsExemptHeader correctly.
TEST_F(ProfileIOSTest, CorsExemptHeader) {
  web::WebTaskEnvironment task_environment;
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  network::mojom::NetworkContextParamsPtr expected_params =
      network::mojom::NetworkContextParams::New();
  variations::UpdateCorsExemptHeaderForVariations(expected_params.get());

  network::mojom::NetworkContextParamsPtr actual_params =
      network::mojom::NetworkContextParams::New();
  profile.get()->UpdateCorsExemptHeader(actual_params.get());

  ASSERT_EQ(expected_params->cors_exempt_header_list.size(),
            actual_params->cors_exempt_header_list.size());
  for (size_t i = 0; i < expected_params->cors_exempt_header_list.size(); ++i) {
    EXPECT_EQ(expected_params->cors_exempt_header_list[i],
              actual_params->cors_exempt_header_list[i]);
  }
}

}  // namespace
}  // namespace ios
