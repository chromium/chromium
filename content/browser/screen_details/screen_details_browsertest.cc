// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/screen_details/screen_details_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/display/screen_base.h"

namespace content {

using ScreenDetailsTest = ContentBrowserTest;

// Test ScreenDetails API promise rejection without permission.
IN_PROC_BROWSER_TEST_F(ScreenDetailsTest, GetScreensNoPermission) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(shell(), "'getScreenDetails' in self"));
  // getScreenDetails() rejects its promise without permission.
  EXPECT_THAT(EvalJs(shell(), "await getScreenDetails()"),
              EvalJsResult::IsError());
}

// TODO(crbug.com/40145721): Test ScreenDetails API values with permission.
IN_PROC_BROWSER_TEST_F(ScreenDetailsTest, DISABLED_GetScreensBasic) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(shell(), "'getScreenDetails' in self"));
  auto result = EvalJs(shell(), content::test::kGetScreenDetailsScript);
  EXPECT_EQ(content::test::GetExpectedScreenDetails(), result.value);
}

// Test that screen.isExtended matches the availability of multiple displays.
IN_PROC_BROWSER_TEST_F(ScreenDetailsTest, IsExtendedBasic) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(shell(), "'isExtended' in screen"));
  EXPECT_EQ("boolean", EvalJs(shell(), "typeof screen.isExtended"));
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays() > 1,
            EvalJs(shell(), "screen.isExtended"));
}

// Test ScreenDetails functionality with a fake display::Screen object.
class FakeScreenDetailsTest : public ScreenDetailsTest {
 public:
  FakeScreenDetailsTest() = default;
  ~FakeScreenDetailsTest() override = default;
  FakeScreenDetailsTest(const FakeScreenDetailsTest&) = delete;
  void operator=(const FakeScreenDetailsTest&) = delete;

 protected:
  // ScreenDetailsTest:
  void SetUp() override {
    display::Screen::SetScreenInstance(&screen_);
    screen()->display_list().AddDisplay({0, gfx::Rect(100, 100, 801, 802)},
                                        display::DisplayList::Type::PRIMARY);

    ScreenDetailsTest::SetUp();
  }
  void TearDown() override {
    ScreenDetailsTest::TearDown();
    display::Screen::SetScreenInstance(nullptr);
  }

  void SetUpOnMainThread() override {
    ScreenDetailsTest::SetUpOnMainThread();
    // Create a shell that observes the fake screen.
    test_shell_ = CreateBrowser();
  }

  void TearDownOnMainThread() override {
    test_shell_ = nullptr;
    ScreenDetailsTest::TearDownOnMainThread();
  }

  display::ScreenBase* screen() { return &screen_; }
  Shell* test_shell() { return test_shell_; }

 private:
  display::ScreenBase screen_;
  raw_ptr<Shell> test_shell_ = nullptr;
};

// TODO(crbug.com/40115071): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/40115071): Android requires a GetDisplayNearestView overload.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_GetScreensFaked DISABLED_GetScreensFaked
#else
#define MAYBE_GetScreensFaked GetScreensFaked
#endif
IN_PROC_BROWSER_TEST_F(FakeScreenDetailsTest, MAYBE_GetScreensFaked) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(test_shell(), "'getScreenDetails' in self"));

  content::WebContents* contents = test_shell()->web_contents();
  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(
          contents->GetBrowserContext());
  permission_controller->GrantPermissionOverrides(
      contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
      {blink::PermissionType::WINDOW_MANAGEMENT});

  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  display::Display display(2, gfx::Rect(901, 100, 801, 802));
  // Test that UTF-8 characters are correctly reflected in the JS output.
  display.set_label("Inbyggd skärm");
  screen()->display_list().AddDisplay(display,
                                      display::DisplayList::Type::NOT_PRIMARY);

  EvalJsResult result =
      EvalJs(test_shell(), content::test::kGetScreenDetailsScript);
  EXPECT_EQ(content::test::GetExpectedScreenDetails(), result.value);
}

// TODO(crbug.com/40115071): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/40115071): Android requires a GetDisplayNearestView overload.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IsExtendedFaked DISABLED_IsExtendedFaked
#else
#define MAYBE_IsExtendedFaked IsExtendedFaked
#endif
IN_PROC_BROWSER_TEST_F(FakeScreenDetailsTest, MAYBE_IsExtendedFaked) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  EXPECT_FALSE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());

  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_TRUE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());

  screen()->display_list().RemoveDisplay(1);
  EXPECT_FALSE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
}

// TODO(crbug.com/40115071): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/40115071): Android requires a GetDisplayNearestView overload.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_ScreenOnchangeNoPermission DISABLED_ScreenOnchangeNoPermission
#else
#define MAYBE_ScreenOnchangeNoPermission ScreenOnchangeNoPermission
#endif
// Sites with no permission only get an event if screen.isExtended changes.
// TODO(crbug.com/40145721): Need content_browsertests permission controls.
IN_PROC_BROWSER_TEST_F(FakeScreenDetailsTest,
                       MAYBE_ScreenOnchangeNoPermission) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(test_shell(), "'onchange' in screen"));
  constexpr char kSetScreenOnchange[] = R"(
    window.screen.onchange = function() { ++document.title; };
    document.title = 0;
  )";
  EXPECT_EQ(0, EvalJs(test_shell(), kSetScreenOnchange));
  EXPECT_FALSE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("0", EvalJs(test_shell(), "document.title"));

  // screen.isExtended changes from false to true here, so an event is sent.
  EXPECT_FALSE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_TRUE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // screen.isExtended remains unchanged, so no event is sent.
  screen()->display_list().AddDisplay({2, gfx::Rect(901, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_TRUE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // screen.isExtended remains unchanged, so no event is sent.
  EXPECT_NE(0u, screen()->display_list().UpdateDisplay(
                    {2, gfx::Rect(902, 100, 801, 802)}));
  EXPECT_TRUE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // screen.isExtended remains unchanged, so no event is sent.
  screen()->display_list().RemoveDisplay(2);
  EXPECT_TRUE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // screen.isExtended changes from true to false here, so an event is sent.
  screen()->display_list().RemoveDisplay(1);
  EXPECT_FALSE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("2", EvalJs(test_shell(), "document.title"));
}

// TODO(crbug.com/40115071): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/40115071): Android requires a GetDisplayNearestView overload.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_ScreenOnChangeForIsExtended DISABLED_ScreenOnChangeForIsExtended
#else
#define MAYBE_ScreenOnChangeForIsExtended ScreenOnChangeForIsExtended
#endif
// Sites should get Screen.change events anytime Screen.isExtended changes.
IN_PROC_BROWSER_TEST_F(FakeScreenDetailsTest,
                       MAYBE_ScreenOnChangeForIsExtended) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(test_shell(), "'onchange' in screen"));
  constexpr char kSetScreenOnChange[] = R"(
    screen.onchange = function() { ++document.title; };
    document.title = 0;
  )";
  EXPECT_EQ(0, EvalJs(test_shell(), kSetScreenOnChange));
  EXPECT_EQ("0", EvalJs(test_shell(), "document.title"));

  // Screen.isExtended changes from false to true here, so an event is sent.
  EXPECT_FALSE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_TRUE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // The current Screen remains unchanged, so no event is sent.
  screen()->display_list().AddDisplay({2, gfx::Rect(901, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_TRUE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // The current Screen remains unchanged, so no event is sent.
  EXPECT_NE(0u, screen()->display_list().UpdateDisplay(
                    {2, gfx::Rect(902, 100, 801, 802)}));
  EXPECT_TRUE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // The current Screen remains unchanged, so no event is sent.
  screen()->display_list().RemoveDisplay(2);
  EXPECT_TRUE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // Screen.isExtended changes from true to false here, so an event is sent.
  screen()->display_list().RemoveDisplay(1);
  EXPECT_FALSE(EvalJs(test_shell(), "screen.isExtended").ExtractBool());
  EXPECT_EQ("2", EvalJs(test_shell(), "document.title"));
}

// TODO(crbug.com/40115071): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/40115071): Android requires a GetDisplayNearestView overload.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_ScreenOnChangeForAttributes DISABLED_ScreenOnChangeForAttributes
#else
#define MAYBE_ScreenOnChangeForAttributes ScreenOnChangeForAttributes
#endif
// Sites should get Screen.change events anytime other Screen attributes change.
IN_PROC_BROWSER_TEST_F(FakeScreenDetailsTest,
                       MAYBE_ScreenOnChangeForAttributes) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(test_shell(), "'onchange' in screen"));
  constexpr char kSetScreenOnChange[] = R"(
    screen.onchange = function() { ++document.title; };
    document.title = 0;
  )";
  EXPECT_EQ(0, EvalJs(test_shell(), kSetScreenOnChange));
  EXPECT_EQ("0", EvalJs(test_shell(), "document.title"));

  // An event is sent when Screen work area changes.
  // work_area translates into Screen.available_rect.
  display::Display display = screen()->display_list().displays()[0];
  display.set_work_area(gfx::Rect(101, 102, 903, 904));
  EXPECT_NE(0u, screen()->display_list().UpdateDisplay(display));
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // An event is sent when Screen scaling changes.
  display.set_device_scale_factor(display.device_scale_factor() * 2);
  EXPECT_NE(0u, screen()->display_list().UpdateDisplay(display));
  EXPECT_EQ("2", EvalJs(test_shell(), "document.title"));
}

}  // namespace content
