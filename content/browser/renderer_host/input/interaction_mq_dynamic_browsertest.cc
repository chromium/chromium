// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

namespace {

class InteractionMediaQueriesDynamicTest : public ContentBrowserTest {
 public:
  InteractionMediaQueriesDynamicTest() = default;
  ~InteractionMediaQueriesDynamicTest() override = default;
};

}  //  namespace

// Disable test on Android ASAN bot: crbug.com/807420
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    (BUILDFLAG(IS_ANDROID) && !defined(ADDRESS_SANITIZER))
IN_PROC_BROWSER_TEST_F(InteractionMediaQueriesDynamicTest,
                       PointerMediaQueriesDynamic) {
  ui::SetAvailablePointerAndHoverTypesForTesting(ui::POINTER_TYPE_NONE,
                                                 ui::HOVER_TYPE_NONE);
  SlowWebPreferenceCache::GetInstance()->OnInputDeviceConfigurationChanged(0);

  GURL test_url = GetTestUrl("", "interaction-mq-dynamic.html");
  const std::u16string kSuccessTitle(u"SUCCESS");
  TitleWatcher title_watcher(shell()->web_contents(), kSuccessTitle);
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  ui::SetAvailablePointerAndHoverTypesForTesting(ui::POINTER_TYPE_COARSE,
                                                 ui::HOVER_TYPE_HOVER);
  SlowWebPreferenceCache::GetInstance()->OnInputDeviceConfigurationChanged(0);
  EXPECT_EQ(kSuccessTitle, title_watcher.WaitAndGetTitle());
}
#endif

}  //  namespace content
