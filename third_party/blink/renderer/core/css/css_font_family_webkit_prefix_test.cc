// Copyright 2021 The Chromium Authors
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

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/public/web/win/web_font_rendering.h"
#endif

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

  GenericFontFamilySettings& GetGenericGenericFontFamilySettings() {
    return GetDocument()
        .GetFrame()
        ->GetPage()
        ->GetSettings()
        .GetGenericFontFamilySettings();
  }

  void SetUp() override {
    SimTest::SetUp();
    m_standard_font = GetGenericGenericFontFamilySettings().Standard();
#if BUILDFLAG(IS_WIN)
    // An extra step is required to ensure that the system font is configured.
    // TODO(crbug.com/969622): Remove this.
    blink::WebFontRendering::SetMenuFontMetrics(
        blink::WebString::FromASCII("Arial"), 12);
#endif
  }

  void TearDown() override {
    GetGenericGenericFontFamilySettings().UpdateStandard(m_standard_font);
    SimTest::TearDown();
  }

 private:
  AtomicString m_standard_font;
};

TEST_F(CSSFontFamilyWebKitPrefixTest,
       CSSFontFamilyWebKitPrefixTest_WebKitBodyFontBuilder) {
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontBuilderCSSFontFamilyWebKitPrefixBody));

  // If empty standard font is specified, counter is never triggered.
  GetGenericGenericFontFamilySettings().UpdateStandard(g_empty_atom);
  LoadPageWithFontFamilyValue("initial");
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontBuilderCSSFontFamilyWebKitPrefixBody));
  LoadPageWithFontFamilyValue("-webkit-body");
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontBuilderCSSFontFamilyWebKitPrefixBody));
  LoadPageWithFontFamilyValue("-webkit-body, serif");
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontBuilderCSSFontFamilyWebKitPrefixBody));
  LoadPageWithFontFamilyValue("serif, -webkit-body");
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontBuilderCSSFontFamilyWebKitPrefixBody));

  // This counter is triggered in FontBuilder when -webkit-body is replaced with
  // a non-empty GenericFontFamilySettings's standard font.
  GetGenericGenericFontFamilySettings().UpdateStandard(
      AtomicString("MyStandardFont"));
  LoadPageWithFontFamilyValue("initial");
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontBuilderCSSFontFamilyWebKitPrefixBody));
  LoadPageWithFontFamilyValue("-webkit-body, serif");
  ASSERT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFontBuilderCSSFontFamilyWebKitPrefixBody));
}

TEST_F(CSSFontFamilyWebKitPrefixTest,
       CSSFontFamilyWebKitPrefixTest_WebKitBodyFontSelector) {
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixBody));

  // If empty standard font is specified, counter is never triggered.
  GetGenericGenericFontFamilySettings().UpdateStandard(g_empty_atom);

  for (String font_family_value :
       {"initial", "-webkit-body", "-webkit-body, serif",
        "serif, -webkit-body"}) {
    LoadPageWithFontFamilyValue(font_family_value);
    ASSERT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixBody))
        << "font-family: " << font_family_value
        << "; lead to counting use of -webkit-body generic family despite "
           "generic family being configured to empty family name in settings.";
  }

  // Implementation via FontDescription::GenericFamilyType is weird, here the
  // last specified generic family is set by FontBuilder. So FontSelector will
  // only trigger the counter if -webkit-body is at the last position.
  GetGenericGenericFontFamilySettings().UpdateStandard(
      AtomicString("MyStandardFont"));
  LoadPageWithFontFamilyValue("initial");
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixBody));
  LoadPageWithFontFamilyValue("-webkit-body, serif");
  ASSERT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixBody));
  LoadPageWithFontFamilyValue("serif, -webkit-body");
  ASSERT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixBody));
}

TEST_F(CSSFontFamilyWebKitPrefixTest,
       CSSFontFamilyWebKitPrefixTest_BlinkMacSystemFont) {
  ASSERT_FALSE(GetDocument().IsUseCounted(WebFeature::kBlinkMacSystemFont));

  // Counter should be not be triggered if system-ui is placed before.
  LoadPageWithFontFamilyValue("system-ui, BlinkMacSystemFont");
  ASSERT_FALSE(GetDocument().IsUseCounted(WebFeature::kBlinkMacSystemFont));

  // Counter should be triggered on macOS, even if -apple-system is placed
  // before or -system-ui is place after.
  LoadPageWithFontFamilyValue("-apple-system, BlinkMacSystemFont, system-ui");
#if BUILDFLAG(IS_MAC)
  ASSERT_TRUE(GetDocument().IsUseCounted(WebFeature::kBlinkMacSystemFont));
#else
  ASSERT_FALSE(GetDocument().IsUseCounted(WebFeature::kBlinkMacSystemFont));
#endif
}

}  // namespace blink
