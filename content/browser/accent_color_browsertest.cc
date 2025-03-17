// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#import "content/browser/theme_helper_mac.h"
#include "third_party/blink/public/common/sandbox_support/sandbox_support_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace content {

// Test that the System AccentColor keyword is supported ONLY for installed
// WebApps. Currently this test is applied ONLY for Windows,ChromeOS and Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

enum class AppType { WebApp, NoneWebApp };

class SystemAccentColorTest : public ContentBrowserTest {
 protected:
  enum class TestType { InstalledWebApp, NonWebApp };
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    browser_client_ = std::make_unique<BrowserClientForAccentColorTest>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "CSSAccentColorKeyword");
  }

  void SetWebAppScope(const GURL web_app_scope) {
    browser_client_->set_web_app_scope(web_app_scope);
    shell()->web_contents()->OnWebPreferencesChanged();
  }

  void SetAccentColor(SkColor accent_color) {
#if BUILDFLAG(IS_MAC)
    // Mac uses a different pipeline to get the accent color, so we need to
    // update the color through the ThemeHelperMac.
    // Windows && ChromeOS pipeline：
    // system color ---set to ---> NativeTheme <---get from--- css
    // MacOS pipeline:
    // system color ---set to---> ThemeHelperMac ---duplicate to--->
    // SandboxSupport <---get from--- css
    ThemeHelperMac::GetInstance()->SetAccentColorForTesting(accent_color);
#else
    ui::NativeTheme::GetInstanceForWeb()->set_user_color(accent_color);
    ui::NativeTheme::GetInstanceForWeb()->NotifyOnNativeThemeUpdated();
#endif  // BUILDFLAG(IS_MAC)
  }

  void SetUpTestPageWithAccentColor(TestType type, SkColor accent_color) {
    GURL web_app_scope = GURL(
        "data:text/html,<body style='background-color: "
        "AccentColor;'> System Accent Color </body>");
    SetWebAppScope(type == TestType::InstalledWebApp ? web_app_scope : GURL());

    SetAccentColor(accent_color);

    ASSERT_TRUE(NavigateToURL(shell(), web_app_scope));
    ASSERT_EQ("System Accent Color",
              EvalJs(shell(), "document.body.innerText"));
  }

  EvalJsResult GetBodyBackgroundColor() {
    return EvalJs(shell(),
                  "window.getComputedStyle(document.body).backgroundColor");
  }

 private:
  class BrowserClientForAccentColorTest
      : public ContentBrowserTestContentBrowserClient {
   public:
    explicit BrowserClientForAccentColorTest() {}

    void set_web_app_scope(const GURL& web_app_scope) {
      web_app_scope_ = web_app_scope;
    }

    void OverrideWebPreferences(
        WebContents* web_contents,
        SiteInstance& main_frame_site,
        blink::web_pref::WebPreferences* web_prefs) override {
      ContentBrowserTestContentBrowserClient::OverrideWebPreferences(
          web_contents, main_frame_site, web_prefs);

      web_prefs->web_app_scope = web_app_scope_;
    }

   private:
    GURL web_app_scope_;
  };

  std::unique_ptr<BrowserClientForAccentColorTest> browser_client_;
};

IN_PROC_BROWSER_TEST_F(SystemAccentColorTest,
                       SystemAccentColorKeywordForInstalledWebApp) {
  SetUpTestPageWithAccentColor(TestType::InstalledWebApp,
                               SkColorSetRGB(135, 115, 10));
  // For installed WebApps we expect System AccentColor keyword resolve to
  // OS-defined accent-color, which are currently pumped for ChromeOS,
  // Windows and Mac.
  EXPECT_EQ("rgb(135, 115, 10)", GetBodyBackgroundColor());
}

IN_PROC_BROWSER_TEST_F(SystemAccentColorTest,
                       SystemAccentColorKeywordForNonWebApp) {
  SetUpTestPageWithAccentColor(TestType::NonWebApp,
                               SkColorSetRGB(135, 115, 10));
  // System AccentColor keyword returns a hard coded value (shade of blue) for
  // non-installed websites.
  EXPECT_EQ("rgb(0, 117, 255)", GetBodyBackgroundColor());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

}  // namespace content
