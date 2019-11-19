// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

#include "components/variations/net/variations_http_headers.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/web/public/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ios {
namespace {

using ChromeBrowserStateTest = web::WebTest;

// Tests that ChromeBrowserState implements UpdateCorsExemptHeader correctly.
TEST_F(ChromeBrowserStateTest, CorsExemptHeader) {
  TestChromeBrowserState::Builder builder;
  std::unique_ptr<TestChromeBrowserState> browser_state = builder.Build();
  ASSERT_TRUE(browser_state);

  network::mojom::NetworkContextParamsPtr expected_params =
      network::mojom::NetworkContextParams::New();
  variations::UpdateCorsExemptHeaderForVariations(expected_params.get());

  network::mojom::NetworkContextParamsPtr actual_params =
      network::mojom::NetworkContextParams::New();
  browser_state->UpdateCorsExemptHeader(actual_params.get());

  ASSERT_EQ(expected_params->cors_exempt_header_list.size(),
            actual_params->cors_exempt_header_list.size());
  for (size_t i = 0; i < expected_params->cors_exempt_header_list.size(); ++i) {
    EXPECT_EQ(expected_params->cors_exempt_header_list[i],
              actual_params->cors_exempt_header_list[i]);
  }
}

}  // namespace
}  // namespace ios
