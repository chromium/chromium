// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fingerprinting_protection/canvas_noise_token_data.h"

#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class CanvasNoiseTokenDataBrowserTest : public content::ContentBrowserTest {
 public:
  CanvasNoiseTokenDataBrowserTest() = default;

  void TearDown() override { scoped_feature_list_.Reset(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      fingerprinting_protection_interventions::features::kCanvasNoise};
};

IN_PROC_BROWSER_TEST_F(CanvasNoiseTokenDataBrowserTest,
                       DifferentBrowserContextDifferCanvasNoiseTokens) {
  blink::NoiseToken normal_token = CanvasNoiseTokenData::GetToken(
      CreateBrowser()->web_contents()->GetBrowserContext(),
      url::Origin::Create(GURL("https://example.test")));
  blink::NoiseToken incognito_token = CanvasNoiseTokenData::GetToken(
      CreateOffTheRecordBrowser()->web_contents()->GetBrowserContext(),
      url::Origin::Create(GURL("https://example.test")));

  EXPECT_NE(normal_token.Value(), 0UL);
  EXPECT_NE(incognito_token.Value(), 0UL);
  EXPECT_NE(normal_token, incognito_token);
}

}  // namespace content
