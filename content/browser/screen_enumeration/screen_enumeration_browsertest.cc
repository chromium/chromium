// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
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

// Used to get async getScreenDetails() info in a list of dictionary values.
constexpr char kGetScreensScript[] = R"(
  (async () => {
    const screenDetails = await self.getScreenDetails();
    let result = [];
    for (let s of screenDetails.screens) {
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

// Returns a list of dictionary values from native screen information, intended
// for comparison with the result of kGetScreensScript.
base::Value GetExpectedScreens() {
  base::Value expected_screens(base::Value::Type::LIST);
  auto* screen = display::Screen::GetScreen();
  size_t id = 0;
  for (const auto& d : screen->GetAllDisplays()) {
    base::Value s(base::Value::Type::DICTIONARY);
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

IN_PROC_BROWSER_TEST_F(ScreenEnumerationTest, GetScreensNoPermission) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(shell(), "'getScreenDetails' in self"));
  // getScreenDetails() rejects its promise without the WindowPlacement
  // permission.
  EXPECT_FALSE(EvalJs(shell(), "await getScreenDetails()").error.empty());
}

// TODO(crbug.com/1119974): Need content_browsertests permission controls.
IN_PROC_BROWSER_TEST_F(ScreenEnumerationTest, DISABLED_GetScreensBasic) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(shell(), "'getScreenDetails' in self"));
  auto result = EvalJs(shell(), kGetScreensScript);
  EXPECT_EQ(GetExpectedScreens(), result.value);
}

IN_PROC_BROWSER_TEST_F(ScreenEnumerationTest, IsExtendedBasic) {
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(shell(), "'isExtended' in screen"));
  EXPECT_EQ("boolean", EvalJs(shell(), "typeof screen.isExtended"));
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays() > 1,
            EvalJs(shell(), "screen.isExtended"));
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
  raw_ptr<display::Screen> original_screen_ = nullptr;
  display::ScreenBase screen_;
  raw_ptr<Shell> test_shell_ = nullptr;
};

// TODO(crbug.com/1042990): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/1042990): Android requires a GetDisplayNearestView overload.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_GetScreensFaked DISABLED_GetScreensFaked
#else
// TODO(crbug.com/1119974): Need content_browsertests permission controls.
#define MAYBE_GetScreensFaked DISABLED_GetScreensFaked
#endif
IN_PROC_BROWSER_TEST_F(FakeScreenEnumerationTest, MAYBE_GetScreensFaked) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(test_shell(), "'getScreenDetails' in self"));

  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  screen()->display_list().AddDisplay({2, gfx::Rect(901, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);

  auto result = EvalJs(test_shell(), kGetScreensScript);
  EXPECT_EQ(GetExpectedScreens(), result.value);
}

// TODO(crbug.com/1042990): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/1042990): Android requires a GetDisplayNearestView overload.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_IsExtendedFaked DISABLED_IsExtendedFaked
#else
#define MAYBE_IsExtendedFaked IsExtendedFaked
#endif
IN_PROC_BROWSER_TEST_F(FakeScreenEnumerationTest, MAYBE_IsExtendedFaked) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  EXPECT_EQ(false, EvalJs(test_shell(), "screen.isExtended"));

  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_EQ(true, EvalJs(test_shell(), "screen.isExtended"));

  screen()->display_list().RemoveDisplay(1);
  EXPECT_EQ(false, EvalJs(test_shell(), "screen.isExtended"));
}

// TODO(crbug.com/1042990): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/1042990): Android requires a GetDisplayNearestView overload.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_ScreenOnchangeNoPermission DISABLED_ScreenOnchangeNoPermission
#else
#define MAYBE_ScreenOnchangeNoPermission ScreenOnchangeNoPermission
#endif
// Sites with no permission only get an event if screen.isExtended changes.
// TODO(crbug.com/1119974): Need content_browsertests permission controls.
IN_PROC_BROWSER_TEST_F(FakeScreenEnumerationTest,
                       MAYBE_ScreenOnchangeNoPermission) {
  ASSERT_TRUE(NavigateToURL(test_shell(), GetTestUrl(nullptr, "empty.html")));
  ASSERT_EQ(true, EvalJs(test_shell(), "'onchange' in screen"));
  constexpr char kSetScreenOnchange[] = R"(
    window.screen.onchange = function() { ++document.title; };
    document.title = 0;
  )";
  EXPECT_EQ(0, EvalJs(test_shell(), kSetScreenOnchange));
  EXPECT_EQ(false, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("0", EvalJs(test_shell(), "document.title"));

  // screen.isExtended changes from false to true here, so an event is sent.
  EXPECT_EQ(false, EvalJs(test_shell(), "screen.isExtended"));
  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_EQ(true, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // screen.isExtended remains unchanged, so no event is sent.
  screen()->display_list().AddDisplay({2, gfx::Rect(901, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_EQ(true, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // screen.isExtended remains unchanged, so no event is sent.
  EXPECT_NE(0u, screen()->display_list().UpdateDisplay(
                    {2, gfx::Rect(902, 100, 801, 802)}));
  EXPECT_EQ(true, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // screen.isExtended remains unchanged, so no event is sent.
  screen()->display_list().RemoveDisplay(2);
  EXPECT_EQ(true, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // screen.isExtended changes from true to false here, so an event is sent.
  screen()->display_list().RemoveDisplay(1);
  EXPECT_EQ(false, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("2", EvalJs(test_shell(), "document.title"));
}

// TODO(crbug.com/1042990): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/1042990): Android requires a GetDisplayNearestView overload.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_ScreenOnChangeForIsExtended DISABLED_ScreenOnChangeForIsExtended
#else
#define MAYBE_ScreenOnChangeForIsExtended ScreenOnChangeForIsExtended
#endif
// Sites should get Screen.change events anytime Screen.isExtended changes.
IN_PROC_BROWSER_TEST_F(FakeScreenEnumerationTest,
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
  EXPECT_EQ(false, EvalJs(test_shell(), "screen.isExtended"));
  screen()->display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_EQ(true, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // The current Screen remains unchanged, so no event is sent.
  screen()->display_list().AddDisplay({2, gfx::Rect(901, 100, 801, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
  EXPECT_EQ(true, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // The current Screen remains unchanged, so no event is sent.
  EXPECT_NE(0u, screen()->display_list().UpdateDisplay(
                    {2, gfx::Rect(902, 100, 801, 802)}));
  EXPECT_EQ(true, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // The current Screen remains unchanged, so no event is sent.
  screen()->display_list().RemoveDisplay(2);
  EXPECT_EQ(true, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("1", EvalJs(test_shell(), "document.title"));

  // Screen.isExtended changes from true to false here, so an event is sent.
  screen()->display_list().RemoveDisplay(1);
  EXPECT_EQ(false, EvalJs(test_shell(), "screen.isExtended"));
  EXPECT_EQ("2", EvalJs(test_shell(), "document.title"));
}

// TODO(crbug.com/1042990): Windows crashes static casting to ScreenWin.
// TODO(crbug.com/1042990): Android requires a GetDisplayNearestView overload.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_ScreenOnChangeForAttributes DISABLED_ScreenOnChangeForAttributes
#else
#define MAYBE_ScreenOnChangeForAttributes ScreenOnChangeForAttributes
#endif
// Sites should get Screen.change events anytime other Screen attributes change.
IN_PROC_BROWSER_TEST_F(FakeScreenEnumerationTest,
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
