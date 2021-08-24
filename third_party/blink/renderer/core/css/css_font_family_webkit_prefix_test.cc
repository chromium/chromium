// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class CSSFontFamilyWebKitPrefixTest : public SimTest {
 public:
  CSSFontFamilyWebKitPrefixTest() = default;

 protected:
  void LoadPageWithFontFamilyValue(const String& value) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(
        "<head>"
        "<style>"
        "body { font-family: " +
        value +
        "; }"
        "</style>"
        "</head>"
        "<body>Styled Text</body>");
    Compositor().BeginFrame();
    test::RunPendingTasks();
  }
};

TEST_F(CSSFontFamilyWebKitPrefixTest,
       CSSFontFamilyWebKitPrefixTest_WebKitPictograph) {
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixPictograph));
  LoadPageWithFontFamilyValue("-webkit-pictograph, serif");
#if defined(OS_ANDROID)
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixPictograph));
#else
  ASSERT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixPictograph));
#endif
}

}  // namespace blink
