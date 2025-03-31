// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/interventions/common/interventions_features.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace content {

class CanvasNoiseTokenDataBrowserTest : public content::ContentBrowserTest {
 public:
  CanvasNoiseTokenDataBrowserTest() = default;

  void SetUpOnMainThread() override {}
  void TearDown() override { scoped_feature_list_.Reset(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      fingerprinting_protection_interventions::features::kCanvasNoise};
};

IN_PROC_BROWSER_TEST_F(CanvasNoiseTokenDataBrowserTest,
                       DifferentBrowserContextDifferCanvasNoiseTokens) {
  auto* normal_tab =
      static_cast<WebContentsImpl*>(CreateBrowser()->web_contents());
  uint64_t normal_token =
      normal_tab->GetMutableRendererPrefs()->canvas_noise_token;

  auto* incognito_tab = static_cast<WebContentsImpl*>(
      CreateOffTheRecordBrowser()->web_contents());
  uint64_t incognito_token =
      incognito_tab->GetMutableRendererPrefs()->canvas_noise_token;

  EXPECT_NE(normal_token, 0UL);
  EXPECT_NE(incognito_token, 0UL);
  EXPECT_NE(normal_token, incognito_token);
}
}  // namespace content
