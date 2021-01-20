// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/display/screen_base.h"

namespace content {

namespace {

// Used to get async getScreens() info in a list of dictionary values.
constexpr char kGetScreensScript[] = R"(
  (async () => {
    const screens = await self.getScreens();
    let result = [];
    for (let s of screens) {
      result.push({ availHeight: s.availHeight,
                    availLeft: s.availLeft,
                    availTop: s.availTop,
                    availWidth: s.availWidth,
                    colorDepth: s.colorDepth,
                    height: s.height,
                    id: s.id,
                    internal: s.internal,
                    left: s.left,
                    orientation: s.orientation != null,
                    pixelDepth: s.pixelDepth,
                    primary: s.primary,
                    scaleFactor: s.scaleFactor,
                    top: s.top,
                    touchSupport: s.touchSupport,
                    width: s.width });
    }
    return result;
  })();
)";

// Used to get the async result of isMultiScreen().
constexpr char kIsMultiScreenScript[] = R"(
  (async () => { return await self.isMultiScreen(); })();
)";

// Returns a list of dictionary values from native screen information, intended
// for comparison with the result of kGetScreensScript.
base::ListValue GetExpectedScreens() {
  base::ListValue expected_screens;
  auto* screen = display::Screen::GetScreen();
  size_t id = 0;
  for (const auto& d : screen->GetAllDisplays()) {
    base::DictionaryValue s;
    s.SetIntKey("availHeight", d.work_area().height());
    s.SetIntKey("availLeft", d.work_area().x());
    s.SetIntKey("availTop", d.work_area().y());
    s.SetIntKey("availWidth", d.work_area().width());
    s.SetIntKey("colorDepth", d.color_depth());
    s.SetIntKey("height", d.bounds().height());
    s.SetStringKey("id", base::NumberToString(id++));
    s.SetBoolKey("internal", d.IsInternal());
    s.SetIntKey("left", d.bounds().x());
    s.SetBoolKey("orientation", false);
    s.SetIntKey("pixelDepth", d.color_depth());
    s.SetBoolKey("primary", d.id() == screen->GetPrimaryDisplay().id());
    // Handle JS's pattern for specifying integer and floating point numbers.
    int int_scale_factor = base::ClampCeil(d.device_scale_factor());
    if (int_scale_factor == d.device_scale_factor())
      s.SetIntKey("scaleFactor", int_scale_factor);
    else
      s.SetDoubleKey("scaleFactor", d.device_scale_factor());
    s.SetIntKey("top", d.bounds().y());
    s.SetBoolKey("touchSupport", d.touch_support() ==
                                     display::Display::TouchSupport::AVAILABLE);
    s.SetIntKey("width", d.bounds().width());
    expected_screens.Append(std::move(s));
  }
  return expected_screens;
}

}  // namespace

// Tests screen enumeration aspects of the WindowPlacement feature.
class ScreenEnumerationTest : public ContentBrowserTest {
 public:
  ScreenEnumerationTest() = default;
  ~ScreenEnumerationTest() override = default;
  ScreenEnumerationTest(const ScreenEnumerationTest&) = delete;
  void operator=(const ScreenEnumerationTest&) = delete;

 protected:
  // ContentBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "WindowPlacement");
    ContentBrowserTest::SetUpCommandLine(command_line);
  }
};

// TODO(crbug.com/1119974): Need content_browsertests permission controls.
IN_PROC_BROWSER_TEST_F(ScreenEnumerationTest, DISABLED_GetScreensBasic) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(shell()->web_contents(), "'getScreens' in self"));
  auto result = EvalJs(shell()->web_contents(), kGetScreensScript);
  EXPECT_EQ(GetExpectedScreens(), base::Value::AsListValue(result.value));
}

IN_PROC_BROWSER_TEST_F(ScreenEnumerationTest, IsMultiScreenBasic) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(shell()->web_contents(), "'isMultiScreen' in self"));
  auto result = EvalJs(shell()->web_contents(), kIsMultiScreenScript);
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays() > 1,
            result.ExtractBool());
}

// Tests screen enumeration functionality with a fake Screen object.
class FakeScreenEnumerationTest : public ScreenEnumerationTest {
 public:
  FakeScreenEnumerationTest() = default;
  ~FakeScreenEnumerationTest() override = default;
  FakeScreenEnumerationTest(const FakeScreenEnumerationTest&) = delete;
  void operator=(const FakeScreenEnumerationTest&) = delete;

 protected:
  // ScreenEnumerationTest:
  void SetUpOnMainThread() override {
    ScreenEnumerationTest::SetUpOnMainThread();
    original_screen_ = display::Screen::GetScreen();
    display::Screen::SetScreenInstance(&screen_);

    // Create a shell that observes the fake screen. A display is required.
    screen()->display_list().AddDisplay({0, gfx::Rect(100, 100, 801, 802)},
                                        display::DisplayList::Type::PRIMARY);
    test_shell_ = CreateBrowser();
  }
  void TearDownOnMainThread() override {
    display::Screen::SetScreenInstance(original_screen_);
    ScreenEnumerationTest::TearDownOnMainThread();
  }

  display::ScreenBase* screen() { return &screen_; }
  Shell* test_shell() { return test_shell_; }

 private:
  display::Screen* original_screen_ = nullptr;
  display::ScreenBase screen_;
  Shell* test_shell_ = nullptr;
};

// TODO(crbug.com/1042990): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/1042990): Android requires a GetDisplayNearestView overload.
#if defined(OS_ANDROID) || defined(OS_WIN)
#define MAYBE_GetScreensFaked DISABLED_GetScreensFaked
#else
// TODO(crbug.com/1119974): Need content_browsertests permission controls.
#define MAYBE_GetScreensFaked DISABLED_GetScreensFaked
#endif
IN_PROC_BROWSER_TEST_F(FakeScreenEnumerationTest, MAYBE_GetScreensFaked) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(test_shell()->web_contents(), "'getScreens' in self"));

  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  screen()->display_list().AddDisplay({2, gfx::Rect(901, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);

  auto result = EvalJs(test_shell()->web_contents(), kGetScreensScript);
  EXPECT_EQ(GetExpectedScreens(), base::Value::AsListValue(result.value));
}

// TODO(crbug.com/1042990): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/1042990): Android requires a GetDisplayNearestView overload.
#if defined(OS_ANDROID) || defined(OS_WIN)
#define MAYBE_IsMultiScreenFaked DISABLED_IsMultiScreenFaked
#else
#define MAYBE_IsMultiScreenFaked IsMultiScreenFaked
#endif
IN_PROC_BROWSER_TEST_F(FakeScreenEnumerationTest, MAYBE_IsMultiScreenFaked) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true,
            EvalJs(test_shell()->web_contents(), "'isMultiScreen' in self"));
  EXPECT_EQ(false, EvalJs(test_shell()->web_contents(), kIsMultiScreenScript));

  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_EQ(true, EvalJs(test_shell()->web_contents(), kIsMultiScreenScript));

  screen()->display_list().RemoveDisplay(1);
  EXPECT_EQ(false, EvalJs(test_shell()->web_contents(), kIsMultiScreenScript));
}

// TODO(crbug.com/1042990): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/1042990): Android requires a GetDisplayNearestView overload.
#if defined(OS_ANDROID) || defined(OS_WIN)
#define MAYBE_OnScreensChangeNoPermission DISABLED_OnScreensChangeNoPermission
#else
#define MAYBE_OnScreensChangeNoPermission OnScreensChangeNoPermission
#endif
// Sites with no permission only get an event if isMultiScreen() changes.
// TODO(crbug.com/1119974): Need content_browsertests permission controls.
IN_PROC_BROWSER_TEST_F(FakeScreenEnumerationTest,
                       MAYBE_OnScreensChangeNoPermission) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true,
            EvalJs(test_shell()->web_contents(), "'onscreenschange' in self"));
  constexpr char kSetOnScreensChange[] = R"(
    onscreenschange = function() { ++document.title; };
    document.title = 0;
  )";
  EXPECT_EQ(0, EvalJs(test_shell()->web_contents(), kSetOnScreensChange));

  // isMultiScreen() changes from false to true here, so an event is sent.
  EXPECT_EQ(false, EvalJs(test_shell()->web_contents(), kIsMultiScreenScript));
  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_EQ(true, EvalJs(test_shell()->web_contents(), kIsMultiScreenScript));
  EXPECT_EQ("1", EvalJs(test_shell()->web_contents(), "document.title"));

  // isMultiScreen() remains unchanged, so no event is sent.
  screen()->display_list().AddDisplay({2, gfx::Rect(901, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_EQ(true, EvalJs(test_shell()->web_contents(), kIsMultiScreenScript));
  EXPECT_EQ("1", EvalJs(test_shell()->web_contents(), "document.title"));

  // isMultiScreen() remains unchanged, so no event is sent.
  EXPECT_NE(0u, screen()->display_list().UpdateDisplay(
                    {2, gfx::Rect(902, 100, 801, 802)}));
  EXPECT_EQ(true, EvalJs(test_shell()->web_contents(), kIsMultiScreenScript));
  EXPECT_EQ("1", EvalJs(test_shell()->web_contents(), "document.title"));

  // isMultiScreen() remains unchanged, so no event is sent.
  screen()->display_list().RemoveDisplay(2);
  EXPECT_EQ(true, EvalJs(test_shell()->web_contents(), kIsMultiScreenScript));
  EXPECT_EQ("1", EvalJs(test_shell()->web_contents(), "document.title"));

  // isMultiScreen() changes from true to false here, so an event is sent.
  screen()->display_list().RemoveDisplay(1);
  EXPECT_EQ(false, EvalJs(test_shell()->web_contents(), kIsMultiScreenScript));
  EXPECT_EQ("2", EvalJs(test_shell()->web_contents(), "document.title"));
}

}  // namespace content
