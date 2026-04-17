// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string_view>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/web_contents/slow_web_preference_cache.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/base/pointer/pointer_device.h"

namespace content {

using InteractionMediaQueriesDynamicTest = ContentBrowserTest;

// Test that interaction media queries update dynamically when pointer/hover
// capabilities change. On desktop, setting (pointer: coarse) + (hover: hover)
// should produce a primary (pointer: coarse) and primary (hover: hover).
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(InteractionMediaQueriesDynamicTest,
                       PointerMediaQueriesDynamic) {
  std::optional<ui::ScopedSetPointerAndHoverTypesForTesting> scoper(
      std::in_place, ui::POINTER_TYPE_NONE, ui::HOVER_TYPE_NONE);
  SlowWebPreferenceCache::GetInstance()->OnInputDeviceConfigurationChanged(0);
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("", "interaction-mq-dynamic.html")));

  static constexpr std::u16string_view kSuccessTitle = u"SUCCESS";
  TitleWatcher title_watcher(shell()->web_contents(), kSuccessTitle);
  scoper.emplace(ui::POINTER_TYPE_COARSE, ui::HOVER_TYPE_HOVER);
  SlowWebPreferenceCache::GetInstance()->OnInputDeviceConfigurationChanged(0);
  EXPECT_EQ(kSuccessTitle, title_watcher.WaitAndGetTitle());
}
#endif

#if BUILDFLAG(IS_ANDROID)
// On Android, when the primary pointer is touchscreen/coarse, primary hover is
// forced to "none" (see crbug.com/41445959). Validate this dynamic update path.
IN_PROC_BROWSER_TEST_F(InteractionMediaQueriesDynamicTest,
                       PointerMediaQueriesDynamicAndroid) {
  std::optional<ui::ScopedSetPointerAndHoverTypesForTesting> scoper(
      std::in_place, ui::POINTER_TYPE_NONE, ui::HOVER_TYPE_NONE);
  SlowWebPreferenceCache::GetInstance()->OnInputDeviceConfigurationChanged(0);
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestUrl("", "interaction-mq-dynamic-android.html")));

  static constexpr std::u16string_view kSuccessTitle = u"SUCCESS";
  TitleWatcher title_watcher(shell()->web_contents(), kSuccessTitle);
  scoper.emplace(ui::POINTER_TYPE_COARSE, ui::HOVER_TYPE_HOVER);
  SlowWebPreferenceCache::GetInstance()->OnInputDeviceConfigurationChanged(0);
  EXPECT_EQ(kSuccessTitle, title_watcher.WaitAndGetTitle());
}
#endif

}  //  namespace content
