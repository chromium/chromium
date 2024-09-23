// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/settings/cpp/fidl_test_base.h>

#include <optional>
#include <string_view>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/test/test_data.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kCssDark[] = "dark";
constexpr char kCssLight[] = "light";

class ThemeManagerTest : public WebEngineBrowserTest,
                         public fuchsia::settings::testing::Display_TestBase {
 public:
  ThemeManagerTest() { set_test_server_root(base::FilePath(kTestServerRoot)); }
  ~ThemeManagerTest() override = default;

  ThemeManagerTest(const ThemeManagerTest&) = delete;
  ThemeManagerTest& operator=(const ThemeManagerTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    component_context_.emplace(
        base::TestComponentContextForProcess::InitialState::kCloneAll);
    display_binding_.emplace(component_context_->additional_services(), this);

    ASSERT_TRUE(embedded_test_server()->Start());
    WebEngineBrowserTest::SetUpOnMainThread();

    frame_ = FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
    base::RunLoop().RunUntilIdle();

    const std::string kPageTitle = "title 1";
    const GURL kPageUrl = embedded_test_server()->GetURL("/title1.html");

    LoadUrlAndExpectResponse(frame_.GetNavigationController(),
                             fuchsia::web::LoadUrlParams(), kPageUrl.spec());

    fuchsia::web::NavigationState state;
    state.set_is_main_document_loaded(true);
    state.set_title(kPageTitle);
    frame_.navigation_listener().RunUntilNavigationStateMatches(state);
  }

  void TearDownOnMainThread() override {
    frame_ = {};
    WebEngineBrowserTest::TearDownOnMainThread();
  }

  // Reports the system |theme_type| via the Display FIDL service.
  void ReportSystemTheme(fuchsia::settings::ThemeType theme_type) {
    if (!watch_callback_) {
      EXPECT_FALSE(on_watch_closure_);

      base::RunLoop run_loop;
      on_watch_closure_ = run_loop.QuitClosure();
      run_loop.Run();
      ASSERT_TRUE(watch_callback_);
    }

    fuchsia::settings::DisplaySettings settings;
    fuchsia::settings::Theme theme;
    theme.set_theme_type(theme_type);
    settings.set_theme(std::move(theme));
    (*watch_callback_)(std::move(settings));
    watch_callback_ = std::nullopt;
    base::RunLoop().RunUntilIdle();
  }

  // Returns the name of the color scheme selected by the CSS feature matcher.
  std::string_view QueryThemeFromCssFeature() {
    content::WebContents* web_contents =
        context_impl()
            ->GetFrameImplForTest(&frame_.ptr())
            ->web_contents_for_test();

    for (const char* scheme : {kCssDark, kCssLight}) {
      bool matches =
          EvalJs(web_contents,
                 base::StringPrintf(
                     "window.matchMedia('(prefers-color-scheme: %s)').matches",
                     scheme))
              .ExtractBool();

      if (matches)
        return scheme;
    }

    NOTREACHED();
  }

  bool SetTheme(fuchsia::settings::ThemeType theme) {
    fuchsia::web::ContentAreaSettings settings;
    settings.set_theme(theme);
    frame_->SetContentAreaSettings(std::move(settings));
    base::RunLoop().RunUntilIdle();
    return frame_.ptr().is_bound();
  }

 protected:
  // fuchsia::settings::Display implementation.
  void Watch(WatchCallback callback) final {
    watch_callback_ = std::move(callback);

    if (on_watch_closure_)
      std::move(on_watch_closure_).Run();
  }
  void NotImplemented_(const std::string& name) final {
    ADD_FAILURE() << "Unexpected call: " << name;
  }

  std::optional<base::TestComponentContextForProcess> component_context_;
  std::optional<base::ScopedServiceBinding<fuchsia::settings::Display>>
      display_binding_;
  FrameForTest frame_;

  base::OnceClosure on_watch_closure_;
  std::optional<WatchCallback> watch_callback_;
};

IN_PROC_BROWSER_TEST_F(ThemeManagerTest, Default) {
  EXPECT_EQ(QueryThemeFromCssFeature(), kCssLight);
}

IN_PROC_BROWSER_TEST_F(ThemeManagerTest, LightAndDarkRequested) {
  EXPECT_TRUE(SetTheme(fuchsia::settings::ThemeType::DARK));
  EXPECT_EQ(QueryThemeFromCssFeature(), kCssDark);

  EXPECT_TRUE(SetTheme(fuchsia::settings::ThemeType::LIGHT));
  EXPECT_EQ(QueryThemeFromCssFeature(), kCssLight);
}

IN_PROC_BROWSER_TEST_F(ThemeManagerTest, UseDisplayService) {
  SetTheme(fuchsia::settings::ThemeType::DEFAULT);
  base::RunLoop().RunUntilIdle();

  ReportSystemTheme(fuchsia::settings::ThemeType::DARK);
  EXPECT_EQ(QueryThemeFromCssFeature(), kCssDark);
}

// Verify that the Frame connection will drop if the Display service is
// required but missing.
// TODO(crbug.com/40731307): Re-enable this test once the service availability
// validation is back in place.
IN_PROC_BROWSER_TEST_F(ThemeManagerTest, DISABLED_DefaultWithMissingService) {
  SetTheme(fuchsia::settings::ThemeType::DEFAULT);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(display_binding_->has_clients());

  display_binding_ = std::nullopt;
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(display_binding_);
  ASSERT_FALSE(frame_.ptr());
}

// Verify that invalid values from the Display service, such as DEFAULT,
// are discarded in lieu of the fallback light theme.
IN_PROC_BROWSER_TEST_F(ThemeManagerTest, HandleBadInputFromDisplayService) {
  SetTheme(fuchsia::settings::ThemeType::DEFAULT);
  ReportSystemTheme(fuchsia::settings::ThemeType::DEFAULT);

  EXPECT_TRUE(SetTheme(fuchsia::settings::ThemeType::DEFAULT));
  EXPECT_EQ(QueryThemeFromCssFeature(), kCssLight);

  ReportSystemTheme(fuchsia::settings::ThemeType::DEFAULT);
  EXPECT_EQ(QueryThemeFromCssFeature(), kCssLight);
}

}  // namespace
