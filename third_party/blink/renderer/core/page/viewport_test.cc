/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "ui/base/ime/mojom/virtual_keyboard_types.mojom-blink.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class ViewportTest : public testing::Test {
 protected:
  ViewportTest()
      : base_url_("http://www.test.com/"), chrome_url_("chrome://") {}

  ~ViewportTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via the WebViewHelper instance in each test case.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  void RegisterMockedChromeURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via the WebViewHelper instance in each test case.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(chrome_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  void ExecuteScript(WebLocalFrame* frame, const WebString& code) {
    frame->ExecuteScript(WebScriptSource(code));
    blink::test::RunPendingTasks();
  }

  test::TaskEnvironment task_environment_;
  std::string base_url_;
  std::string chrome_url_;

 private:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

static void SetViewportSettings(WebSettings* settings) {
  settings->SetViewportEnabled(true);
  settings->SetViewportMetaEnabled(true);
  settings->SetMainFrameResizesAreOrientationChanges(true);
}

static PageScaleConstraints RunViewportTest(Page* page,
                                            int initial_width,
                                            int initial_height) {
  gfx::Size initial_viewport_size(initial_width, initial_height);
  To<LocalFrame>(page->MainFrame())
      ->View()
      ->SetFrameRect(gfx::Rect(gfx::Point(), initial_viewport_size));
  ViewportDescription description = page->GetViewportDescription();
  PageScaleConstraints constraints = description.Resolve(
      gfx::SizeF(initial_viewport_size), Length::Fixed(980));

  constraints.FitToContentsWidth(constraints.layout_size.width(),
                                 initial_width);
  constraints.ResolveAutoInitialScale();
  return constraints;
}

TEST_F(ViewportTest, viewport6) {
  RegisterMockedHttpURLLoad("viewport/viewport-6.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-6.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(200, constraints.layout_size.width());
  EXPECT_EQ(220, constraints.layout_size.height());
  EXPECT_NEAR(1.6f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.6f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport7) {
  RegisterMockedHttpURLLoad("viewport/viewport-7.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-7.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1280, constraints.layout_size.width());
  EXPECT_EQ(1408, constraints.layout_size.height());
  EXPECT_NEAR(0.25f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport8) {
  RegisterMockedHttpURLLoad("viewport/viewport-8.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-8.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1280, constraints.layout_size.width());
  EXPECT_EQ(1408, constraints.layout_size.height());
  EXPECT_NEAR(0.25f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport9) {
  RegisterMockedHttpURLLoad("viewport/viewport-9.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-9.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1280, constraints.layout_size.width());
  EXPECT_EQ(1408, constraints.layout_size.height());
  EXPECT_NEAR(0.25f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport10) {
  RegisterMockedHttpURLLoad("viewport/viewport-10.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-10.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1280, constraints.layout_size.width());
  EXPECT_EQ(1408, constraints.layout_size.height());
  EXPECT_NEAR(0.25f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport11) {
  RegisterMockedHttpURLLoad("viewport/viewport-11.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-11.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.32f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.32f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(0.5f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport12) {
  RegisterMockedHttpURLLoad("viewport/viewport-12.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-12.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(640, constraints.layout_size.width());
  EXPECT_EQ(704, constraints.layout_size.height());
  EXPECT_NEAR(0.5f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.5f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(0.5f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport13) {
  RegisterMockedHttpURLLoad("viewport/viewport-13.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-13.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1280, constraints.layout_size.width());
  EXPECT_EQ(1408, constraints.layout_size.height());
  EXPECT_NEAR(0.25f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(0.5f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport14) {
  RegisterMockedHttpURLLoad("viewport/viewport-14.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-14.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport15) {
  RegisterMockedHttpURLLoad("viewport/viewport-15.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-15.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport16) {
  RegisterMockedHttpURLLoad("viewport/viewport-16.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-16.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(5.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport17) {
  RegisterMockedHttpURLLoad("viewport/viewport-17.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-17.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(5.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport18) {
  RegisterMockedHttpURLLoad("viewport/viewport-18.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-18.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(64, constraints.layout_size.width());
  EXPECT_NEAR(70.4, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(5.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport19) {
  RegisterMockedHttpURLLoad("viewport/viewport-19.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-19.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(160, constraints.layout_size.width());
  EXPECT_EQ(176, constraints.layout_size.height());
  EXPECT_NEAR(2.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(2.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport20) {
  RegisterMockedHttpURLLoad("viewport/viewport-20.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-20.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(10.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport21) {
  RegisterMockedHttpURLLoad("viewport/viewport-21.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-21.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(10.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport22) {
  RegisterMockedHttpURLLoad("viewport/viewport-22.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-22.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(10.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport23) {
  RegisterMockedHttpURLLoad("viewport/viewport-23.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-23.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(3.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(3.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(3.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport24) {
  RegisterMockedHttpURLLoad("viewport/viewport-24.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-24.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(4.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(4.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(4.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport25) {
  RegisterMockedHttpURLLoad("viewport/viewport-25.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-25.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(10.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport26) {
  RegisterMockedHttpURLLoad("viewport/viewport-26.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-26.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(8.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(8.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(9.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport27) {
  RegisterMockedHttpURLLoad("viewport/viewport-27.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-27.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.32f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.32f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport28) {
  RegisterMockedHttpURLLoad("viewport/viewport-28.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-28.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(352, constraints.layout_size.width());
  EXPECT_NEAR(387.2, constraints.layout_size.height(), 0.01);
  EXPECT_NEAR(0.91f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.91f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport29) {
  RegisterMockedHttpURLLoad("viewport/viewport-29.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-29.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(700, constraints.layout_size.width());
  EXPECT_EQ(770, constraints.layout_size.height());
  EXPECT_NEAR(0.46f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.46f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport30) {
  RegisterMockedHttpURLLoad("viewport/viewport-30.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-30.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(200, constraints.layout_size.width());
  EXPECT_EQ(220, constraints.layout_size.height());
  EXPECT_NEAR(1.6f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.6f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport31) {
  RegisterMockedHttpURLLoad("viewport/viewport-31.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-31.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(700, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport32) {
  RegisterMockedHttpURLLoad("viewport/viewport-32.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-32.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(200, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport33) {
  RegisterMockedHttpURLLoad("viewport/viewport-33.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-33.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(2.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport34) {
  RegisterMockedHttpURLLoad("viewport/viewport-34.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-34.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(640, constraints.layout_size.width());
  EXPECT_EQ(704, constraints.layout_size.height());
  EXPECT_NEAR(0.5f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.5f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport35) {
  RegisterMockedHttpURLLoad("viewport/viewport-35.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-35.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1280, constraints.layout_size.width());
  EXPECT_EQ(1408, constraints.layout_size.height());
  EXPECT_NEAR(0.25f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport36) {
  RegisterMockedHttpURLLoad("viewport/viewport-36.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-36.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_NEAR(636.36, constraints.layout_size.width(), 0.01f);
  EXPECT_EQ(700, constraints.layout_size.height());
  EXPECT_NEAR(1.6f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.50f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport37) {
  RegisterMockedHttpURLLoad("viewport/viewport-37.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-37.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport38) {
  RegisterMockedHttpURLLoad("viewport/viewport-38.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-38.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(640, constraints.layout_size.width());
  EXPECT_EQ(704, constraints.layout_size.height());
  EXPECT_NEAR(0.5f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.5f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport39) {
  RegisterMockedHttpURLLoad("viewport/viewport-39.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-39.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(200, constraints.layout_size.width());
  EXPECT_EQ(700, constraints.layout_size.height());
  EXPECT_NEAR(1.6f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.6f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport40) {
  RegisterMockedHttpURLLoad("viewport/viewport-40.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-40.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(700, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(0.46f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.46f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport41) {
  RegisterMockedHttpURLLoad("viewport/viewport-41.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-41.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1000, constraints.layout_size.width());
  EXPECT_EQ(704, constraints.layout_size.height());
  EXPECT_NEAR(0.5f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.32f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport42) {
  RegisterMockedHttpURLLoad("viewport/viewport-42.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-42.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(1000, constraints.layout_size.height());
  EXPECT_NEAR(2.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport43) {
  RegisterMockedHttpURLLoad("viewport/viewport-43.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-43.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(64, constraints.layout_size.width());
  EXPECT_NEAR(70.4, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(5.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport44) {
  RegisterMockedHttpURLLoad("viewport/viewport-44.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-44.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(10000, constraints.layout_size.width());
  EXPECT_EQ(10000, constraints.layout_size.height());
  EXPECT_NEAR(0.25f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport45) {
  RegisterMockedHttpURLLoad("viewport/viewport-45.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-45.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(3200, constraints.layout_size.width());
  EXPECT_EQ(3520, constraints.layout_size.height());
  EXPECT_NEAR(0.1f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.1f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(0.1f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport46) {
  RegisterMockedHttpURLLoad("viewport/viewport-46.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-46.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(32, constraints.layout_size.width());
  EXPECT_NEAR(35.2, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(10.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport47) {
  RegisterMockedHttpURLLoad("viewport/viewport-47.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-47.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(3000, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport48) {
  RegisterMockedHttpURLLoad("viewport/viewport-48.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-48.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(3000, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport49) {
  RegisterMockedHttpURLLoad("viewport/viewport-49.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-49.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport50) {
  RegisterMockedHttpURLLoad("viewport/viewport-50.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-50.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport51) {
  RegisterMockedHttpURLLoad("viewport/viewport-51.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-51.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport52) {
  RegisterMockedHttpURLLoad("viewport/viewport-52.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-52.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport53) {
  RegisterMockedHttpURLLoad("viewport/viewport-53.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-53.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport54) {
  RegisterMockedHttpURLLoad("viewport/viewport-54.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-54.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport55) {
  RegisterMockedHttpURLLoad("viewport/viewport-55.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-55.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport56) {
  RegisterMockedHttpURLLoad("viewport/viewport-56.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-56.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport57) {
  RegisterMockedHttpURLLoad("viewport/viewport-57.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-57.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport58) {
  RegisterMockedHttpURLLoad("viewport/viewport-58.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-58.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(3200, constraints.layout_size.width());
  EXPECT_EQ(3520, constraints.layout_size.height());
  EXPECT_NEAR(0.1f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.1f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport59) {
  RegisterMockedHttpURLLoad("viewport/viewport-59.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-59.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport60) {
  RegisterMockedHttpURLLoad("viewport/viewport-60.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-60.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(32, constraints.layout_size.width());
  EXPECT_NEAR(35.2, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(10.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport61) {
  RegisterMockedHttpURLLoad("viewport/viewport-61.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-61.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport62) {
  RegisterMockedHttpURLLoad("viewport/viewport-62.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-62.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport63) {
  RegisterMockedHttpURLLoad("viewport/viewport-63.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-63.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport64) {
  RegisterMockedHttpURLLoad("viewport/viewport-64.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-64.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport65) {
  RegisterMockedHttpURLLoad("viewport/viewport-65.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-65.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(100, constraints.layout_size.width());
  EXPECT_EQ(110, constraints.layout_size.height());
  EXPECT_NEAR(3.2f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(3.2f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport66) {
  RegisterMockedHttpURLLoad("viewport/viewport-66.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-66.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(100, constraints.layout_size.width());
  EXPECT_EQ(110, constraints.layout_size.height());
  EXPECT_NEAR(3.2f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(3.2f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport67) {
  RegisterMockedHttpURLLoad("viewport/viewport-67.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-67.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport68) {
  RegisterMockedHttpURLLoad("viewport/viewport-68.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-68.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport69) {
  RegisterMockedHttpURLLoad("viewport/viewport-69.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-69.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(100, constraints.layout_size.width());
  EXPECT_EQ(110, constraints.layout_size.height());
  EXPECT_NEAR(3.2f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(3.2f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport70) {
  RegisterMockedHttpURLLoad("viewport/viewport-70.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-70.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(100, constraints.layout_size.width());
  EXPECT_EQ(110, constraints.layout_size.height());
  EXPECT_NEAR(3.2f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(3.2f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport71) {
  RegisterMockedHttpURLLoad("viewport/viewport-71.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-71.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport72) {
  RegisterMockedHttpURLLoad("viewport/viewport-72.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-72.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(100, constraints.layout_size.width());
  EXPECT_EQ(110, constraints.layout_size.height());
  EXPECT_NEAR(3.2f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(3.2f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport73) {
  RegisterMockedHttpURLLoad("viewport/viewport-73.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-73.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(100, constraints.layout_size.width());
  EXPECT_EQ(110, constraints.layout_size.height());
  EXPECT_NEAR(3.2f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(3.2f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport74) {
  RegisterMockedHttpURLLoad("viewport/viewport-74.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-74.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(100, constraints.layout_size.width());
  EXPECT_EQ(110, constraints.layout_size.height());
  EXPECT_NEAR(3.2f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(3.2f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport75) {
  RegisterMockedHttpURLLoad("viewport/viewport-75.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-75.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(64, constraints.layout_size.width());
  EXPECT_NEAR(70.4, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(5.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport76) {
  RegisterMockedHttpURLLoad("viewport/viewport-76.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-76.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(32, constraints.layout_size.width());
  EXPECT_NEAR(35.2, constraints.layout_size.height(), 0.01);
  EXPECT_NEAR(10.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport77) {
  RegisterMockedHttpURLLoad("viewport/viewport-77.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-77.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1280, constraints.layout_size.width());
  EXPECT_EQ(1408, constraints.layout_size.height());
  EXPECT_NEAR(0.25f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport78) {
  RegisterMockedHttpURLLoad("viewport/viewport-78.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-78.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(100, constraints.layout_size.width());
  EXPECT_EQ(110, constraints.layout_size.height());
  EXPECT_NEAR(3.2f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(3.2f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport79) {
  RegisterMockedHttpURLLoad("viewport/viewport-79.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-79.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport80) {
  RegisterMockedHttpURLLoad("viewport/viewport-80.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-80.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport81) {
  RegisterMockedHttpURLLoad("viewport/viewport-81.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-81.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(3000, constraints.layout_size.width());
  EXPECT_EQ(3300, constraints.layout_size.height());
  EXPECT_NEAR(0.25f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport82) {
  RegisterMockedHttpURLLoad("viewport/viewport-82.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-82.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(400, constraints.layout_size.width());
  EXPECT_EQ(440, constraints.layout_size.height());
  EXPECT_NEAR(0.8f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.8f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport83) {
  RegisterMockedHttpURLLoad("viewport/viewport-83.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-83.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport84) {
  RegisterMockedHttpURLLoad("viewport/viewport-84.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-84.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(480, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport85) {
  RegisterMockedHttpURLLoad("viewport/viewport-85.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-85.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(540, constraints.layout_size.width());
  EXPECT_EQ(594, constraints.layout_size.height());
  EXPECT_NEAR(0.59f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.59f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport86) {
  RegisterMockedHttpURLLoad("viewport/viewport-86.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-86.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_NEAR(457.14, constraints.layout_size.width(), 0.01f);
  EXPECT_NEAR(502.86, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.7f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.7f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport87) {
  RegisterMockedHttpURLLoad("viewport/viewport-87.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-87.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport88) {
  RegisterMockedHttpURLLoad("viewport/viewport-88.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-88.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport90) {
  RegisterMockedHttpURLLoad("viewport/viewport-90.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-90.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(700, constraints.layout_size.width());
  EXPECT_EQ(770, constraints.layout_size.height());
  EXPECT_NEAR(0.5f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.46f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport100) {
  RegisterMockedHttpURLLoad("viewport/viewport-100.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-100.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(400, constraints.layout_size.width());
  EXPECT_EQ(440, constraints.layout_size.height());
  EXPECT_NEAR(0.8f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.8f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport101) {
  RegisterMockedHttpURLLoad("viewport/viewport-101.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-101.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(400, constraints.layout_size.width());
  EXPECT_EQ(440, constraints.layout_size.height());
  EXPECT_NEAR(0.8f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.8f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport102) {
  RegisterMockedHttpURLLoad("viewport/viewport-102.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-102.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(400, constraints.layout_size.width());
  EXPECT_EQ(440, constraints.layout_size.height());
  EXPECT_NEAR(0.8f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.8f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport103) {
  RegisterMockedHttpURLLoad("viewport/viewport-103.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-103.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(400, constraints.layout_size.width());
  EXPECT_EQ(440, constraints.layout_size.height());
  EXPECT_NEAR(0.8f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.8f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport104) {
  RegisterMockedHttpURLLoad("viewport/viewport-104.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-104.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport105) {
  RegisterMockedHttpURLLoad("viewport/viewport-105.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-105.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport106) {
  RegisterMockedHttpURLLoad("viewport/viewport-106.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-106.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport107) {
  RegisterMockedHttpURLLoad("viewport/viewport-107.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-107.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport108) {
  RegisterMockedHttpURLLoad("viewport/viewport-108.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-108.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport109) {
  RegisterMockedHttpURLLoad("viewport/viewport-109.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-109.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport110) {
  RegisterMockedHttpURLLoad("viewport/viewport-110.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-110.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport111) {
  RegisterMockedHttpURLLoad("viewport/viewport-111.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-111.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport112) {
  RegisterMockedHttpURLLoad("viewport/viewport-112.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-112.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(400, constraints.layout_size.width());
  EXPECT_EQ(440, constraints.layout_size.height());
  EXPECT_NEAR(0.8f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.8f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport113) {
  RegisterMockedHttpURLLoad("viewport/viewport-113.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-113.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport114) {
  RegisterMockedHttpURLLoad("viewport/viewport-114.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-114.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport115) {
  RegisterMockedHttpURLLoad("viewport/viewport-115.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-115.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(400, constraints.layout_size.width());
  EXPECT_EQ(440, constraints.layout_size.height());
  EXPECT_NEAR(0.8f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.8f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport116) {
  RegisterMockedHttpURLLoad("viewport/viewport-116.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-116.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(400, constraints.layout_size.width());
  EXPECT_EQ(440, constraints.layout_size.height());
  EXPECT_NEAR(0.8f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.8f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport117) {
  RegisterMockedHttpURLLoad("viewport/viewport-117.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-117.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(400, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport118) {
  RegisterMockedHttpURLLoad("viewport/viewport-118.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-118.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport119) {
  RegisterMockedHttpURLLoad("viewport/viewport-119.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-119.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport120) {
  RegisterMockedHttpURLLoad("viewport/viewport-120.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-120.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport121) {
  RegisterMockedHttpURLLoad("viewport/viewport-121.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-121.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport122) {
  RegisterMockedHttpURLLoad("viewport/viewport-122.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-122.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport123) {
  RegisterMockedHttpURLLoad("viewport/viewport-123.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-123.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport124) {
  RegisterMockedHttpURLLoad("viewport/viewport-124.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-124.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport125) {
  RegisterMockedHttpURLLoad("viewport/viewport-125.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-125.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport126) {
  RegisterMockedHttpURLLoad("viewport/viewport-126.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-126.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport127) {
  RegisterMockedHttpURLLoad("viewport/viewport-127.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-127.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport129) {
  RegisterMockedHttpURLLoad("viewport/viewport-129.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-129.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(123, constraints.layout_size.width());
  EXPECT_NEAR(135.3, constraints.layout_size.height(), 0.01f);
  EXPECT_NEAR(2.60f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(2.60f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport130) {
  RegisterMockedHttpURLLoad("viewport/viewport-130.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-130.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport131) {
  RegisterMockedHttpURLLoad("viewport/viewport-131.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-131.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.maximum_scale, 0.01f);
  EXPECT_FALSE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport132) {
  RegisterMockedHttpURLLoad("viewport/viewport-132.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-132.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport133) {
  RegisterMockedHttpURLLoad("viewport/viewport-133.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-133.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(10.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(10.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport134) {
  RegisterMockedHttpURLLoad("viewport/viewport-134.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-134.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(160, constraints.layout_size.width());
  EXPECT_EQ(176, constraints.layout_size.height());
  EXPECT_NEAR(2.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(2.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport135) {
  RegisterMockedHttpURLLoad("viewport/viewport-135.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-135.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport136) {
  RegisterMockedHttpURLLoad("viewport/viewport-136.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-136.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport137) {
  RegisterMockedHttpURLLoad("viewport/viewport-137.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-137.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport138) {
  RegisterMockedHttpURLLoad("viewport/viewport-138.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(base_url_ + "viewport/viewport-138.html",
                                    nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_NEAR(123.0f, constraints.layout_size.width(), 0.01);
  EXPECT_NEAR(135.3f, constraints.layout_size.height(), 0.01);
  EXPECT_NEAR(2.60f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(2.60f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyHandheldFriendly) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-handheldfriendly.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-handheldfriendly.html", nullptr,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

static void SetQuirkViewportSettings(WebSettings* settings) {
  SetViewportSettings(settings);

  // This quirk allows content attributes of meta viewport tags to be merged.
  settings->SetViewportMetaMergeContentQuirk(true);
}

TEST_F(ViewportTest, viewportLegacyMergeQuirk1) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-merge-quirk-1.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-merge-quirk-1.html", nullptr,
      nullptr, SetQuirkViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(640, constraints.layout_size.width());
  EXPECT_EQ(704, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.maximum_scale, 0.01f);
  EXPECT_FALSE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyMergeQuirk2) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-merge-quirk-2.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-merge-quirk-2.html", nullptr,
      nullptr, SetQuirkViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  // This quirk allows content attributes of meta viewport tags to be merged.
  page->GetSettings().SetViewportMetaMergeContentQuirk(true);
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(500, constraints.layout_size.width());
  EXPECT_EQ(550, constraints.layout_size.height());
  EXPECT_NEAR(2.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(2.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(2.0f, constraints.maximum_scale, 0.01f);
  EXPECT_FALSE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyMobileOptimizedMetaWithoutContent) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-mobileoptimized.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-mobileoptimized.html", nullptr,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyMobileOptimizedMetaWith0) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-mobileoptimized-2.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-mobileoptimized-2.html", nullptr,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyMobileOptimizedMetaWith400) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-mobileoptimized-2.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-mobileoptimized-2.html", nullptr,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyOrdering2) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-ordering-2.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-ordering-2.html", nullptr, nullptr,
      SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(300, constraints.layout_size.width());
  EXPECT_EQ(330, constraints.layout_size.height());
  EXPECT_NEAR(1.07f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.07f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyOrdering3) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-ordering-3.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-ordering-3.html", nullptr, nullptr,
      SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(300, constraints.layout_size.width());
  EXPECT_EQ(330, constraints.layout_size.height());
  EXPECT_NEAR(1.07f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.07f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyOrdering4) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-ordering-4.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-ordering-4.html", nullptr, nullptr,
      SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(300, constraints.layout_size.width());
  EXPECT_EQ(330, constraints.layout_size.height());
  EXPECT_NEAR(1.07f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.07f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyOrdering5) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-ordering-5.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-ordering-5.html", nullptr, nullptr,
      SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyOrdering6) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-ordering-6.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-ordering-6.html", nullptr, nullptr,
      SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyOrdering7) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-ordering-7.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-ordering-7.html", nullptr, nullptr,
      SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(300, constraints.layout_size.width());
  EXPECT_EQ(330, constraints.layout_size.height());
  EXPECT_NEAR(1.07f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.07f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyOrdering8) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-ordering-8.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-ordering-8.html", nullptr, nullptr,
      SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();

  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(300, constraints.layout_size.width());
  EXPECT_EQ(330, constraints.layout_size.height());
  EXPECT_NEAR(1.07f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.07f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyDefaultValueChangedByXHTMLMP) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-xhtmlmp.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-xhtmlmp.html", nullptr, nullptr,
      SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest,
       viewportLegacyDefaultValueChangedByXHTMLMPAndOverriddenByMeta) {
  RegisterMockedHttpURLLoad(
      "viewport/viewport-legacy-xhtmlmp-misplaced-doctype.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-xhtmlmp-misplaced-doctype.html",
      nullptr, nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(640, constraints.layout_size.width());
  EXPECT_EQ(704, constraints.layout_size.height());
  EXPECT_NEAR(0.5f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.5f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyXHTMLMPOrdering) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-xhtmlmp-ordering.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-xhtmlmp-ordering.html", nullptr,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(640, constraints.layout_size.width());
  EXPECT_EQ(704, constraints.layout_size.height());
  EXPECT_NEAR(0.5f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.5f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLegacyXHTMLMPRemoveAndAdd) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-xhtmlmp.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-legacy-xhtmlmp.html", nullptr, nullptr,
      SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);

  ExecuteScript(web_view_helper.LocalMainFrame(),
                "originalDoctype = document.doctype;"
                "document.removeChild(originalDoctype);");

  constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);

  ExecuteScript(web_view_helper.LocalMainFrame(),
                "document.insertBefore(originalDoctype, document.firstChild);");

  constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportLimitsAdjustedForNoUserScale) {
  RegisterMockedHttpURLLoad(
      "viewport/viewport-limits-adjusted-for-no-user-scale.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-limits-adjusted-for-no-user-scale.html",
      nullptr, nullptr, SetViewportSettings);

  web_view_helper.GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 10, 10);

  EXPECT_FALSE(page->GetViewportDescription().user_zoom);
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
}

TEST_F(ViewportTest, viewportLimitsAdjustedForUserScale) {
  RegisterMockedHttpURLLoad(
      "viewport/viewport-limits-adjusted-for-user-scale.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-limits-adjusted-for-user-scale.html",
      nullptr, nullptr, SetViewportSettings);
  web_view_helper.GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 10, 10);

  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
}

class ConsoleMessageWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  void DidAddMessageToConsole(const WebConsoleMessage& msg,
                              const WebString& source_name,
                              unsigned source_line,
                              const WebString& stack_trace) override {
    messages.push_back(msg);
  }

  Vector<WebConsoleMessage> messages;
};

TEST_F(ViewportTest, viewportWarnings1) {
  ConsoleMessageWebFrameClient web_frame_client;

  RegisterMockedHttpURLLoad("viewport/viewport-warnings-1.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-warnings-1.html", &web_frame_client,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_TRUE(web_frame_client.messages.empty());

  EXPECT_EQ(320, constraints.layout_size.width());
  EXPECT_EQ(352, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(2.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportWarnings2) {
  ConsoleMessageWebFrameClient web_frame_client;

  RegisterMockedHttpURLLoad("viewport/viewport-warnings-2.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-warnings-2.html", &web_frame_client,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1U, web_frame_client.messages.size());
  EXPECT_EQ(mojom::ConsoleMessageLevel::kWarning,
            web_frame_client.messages[0].level);
  EXPECT_EQ("The key \"wwidth\" is not recognized and ignored.",
            web_frame_client.messages[0].text);

  EXPECT_EQ(980, constraints.layout_size.width());
  EXPECT_EQ(1078, constraints.layout_size.height());
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportWarnings3) {
  ConsoleMessageWebFrameClient web_frame_client;

  RegisterMockedHttpURLLoad("viewport/viewport-warnings-3.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-warnings-3.html", &web_frame_client,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1U, web_frame_client.messages.size());
  EXPECT_EQ(mojom::ConsoleMessageLevel::kWarning,
            web_frame_client.messages[0].level);
  EXPECT_EQ(
      "The value \"unrecognized-width\" for key \"width\" is invalid, and has "
      "been ignored.",
      web_frame_client.messages[0].text);

  EXPECT_NEAR(980, constraints.layout_size.width(), 0.01);
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportWarnings4) {
  ConsoleMessageWebFrameClient web_frame_client;

  RegisterMockedHttpURLLoad("viewport/viewport-warnings-4.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-warnings-4.html", &web_frame_client,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1U, web_frame_client.messages.size());
  EXPECT_EQ(mojom::ConsoleMessageLevel::kWarning,
            web_frame_client.messages[0].level);
  EXPECT_EQ(
      "The value \"123x456\" for key \"width\" was truncated to its numeric "
      "prefix.",
      web_frame_client.messages[0].text);

  EXPECT_NEAR(123.0f, constraints.layout_size.width(), 0.01);
  EXPECT_NEAR(135.3f, constraints.layout_size.height(), 0.01);
  EXPECT_NEAR(2.60f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(2.60f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportWarnings5) {
  ConsoleMessageWebFrameClient web_frame_client;

  RegisterMockedHttpURLLoad("viewport/viewport-warnings-5.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-warnings-5.html", &web_frame_client,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1U, web_frame_client.messages.size());

  EXPECT_EQ(mojom::ConsoleMessageLevel::kWarning,
            web_frame_client.messages[0].level);
  EXPECT_EQ(
      "Error parsing a meta element's content: ';' is not a valid key-value "
      "pair separator. Please use ',' instead.",
      web_frame_client.messages[0].text);

  EXPECT_NEAR(320.0f, constraints.layout_size.width(), 0.01);
  EXPECT_NEAR(352.0f, constraints.layout_size.height(), 0.01);
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.maximum_scale, 0.01f);
  EXPECT_FALSE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportWarnings6) {
  ConsoleMessageWebFrameClient web_frame_client;

  RegisterMockedHttpURLLoad("viewport/viewport-warnings-6.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-warnings-6.html", &web_frame_client,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  PageScaleConstraints constraints = RunViewportTest(page, 320, 352);

  EXPECT_EQ(1U, web_frame_client.messages.size());
  EXPECT_EQ(mojom::ConsoleMessageLevel::kWarning,
            web_frame_client.messages[0].level);
  EXPECT_EQ(
      "The value \"\" for key \"width\" is invalid, and has been ignored.",
      web_frame_client.messages[0].text);

  EXPECT_NEAR(980, constraints.layout_size.width(), 0.01);
  EXPECT_NEAR(1078, constraints.layout_size.height(), 0.01);
  EXPECT_NEAR(0.33f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.33f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewportWarnings7) {
  ConsoleMessageWebFrameClient web_frame_client;

  RegisterMockedHttpURLLoad("viewport/viewport-warnings-7.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-warnings-7.html", &web_frame_client,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  RunViewportTest(page, 320, 352);

  EXPECT_EQ(0U, web_frame_client.messages.size());
}

TEST_F(ViewportTest, viewportWarnings8) {
  ConsoleMessageWebFrameClient web_frame_client;

  RegisterMockedHttpURLLoad("viewport/viewport-warnings-8.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.InitializeAndLoad(
      base_url_ + "viewport/viewport-warnings-8.html", &web_frame_client,
      nullptr, SetViewportSettings);

  Page* page = web_view_helper.GetWebView()->GetPage();
  RunViewportTest(page, 320, 352);

  EXPECT_EQ(0U, web_frame_client.messages.size());
}

TEST_F(ViewportTest, viewport1) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-merge-quirk-1.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeWithSettings(SetQuirkViewportSettings);
  web_view_impl->MainFrameWidget()->SetDeviceScaleFactorForTesting(3.f);
  frame_test_helpers::LoadFrame(
      web_view_impl->MainFrameImpl(),
      base_url_ + "viewport/viewport-legacy-merge-quirk-1.html");

  Page* page = web_view_helper.GetWebView()->GetPage();
  // Initial width and height must be scaled by DSF.
  PageScaleConstraints constraints = RunViewportTest(page, 960, 1056);

  // constraints layout width == 640 * DSF = 1920
  EXPECT_EQ(1920, constraints.layout_size.width());
  // constraints layout height == 704 * DSF = 2112
  EXPECT_EQ(2112, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(1.0f, constraints.maximum_scale, 0.01f);
  EXPECT_FALSE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport2) {
  RegisterMockedHttpURLLoad("viewport/viewport-legacy-merge-quirk-2.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeWithSettings(SetQuirkViewportSettings);
  web_view_impl->MainFrameWidget()->SetDeviceScaleFactorForTesting(3.f);
  frame_test_helpers::LoadFrame(
      web_view_impl->MainFrameImpl(),
      base_url_ + "viewport/viewport-legacy-merge-quirk-2.html");
  Page* page = web_view_helper.GetWebView()->GetPage();

  // This quirk allows content attributes of meta viewport tags to be merged.
  page->GetSettings().SetViewportMetaMergeContentQuirk(true);
  // Initial width and height must be scaled by DSF.
  PageScaleConstraints constraints = RunViewportTest(page, 960, 1056);

  // constraints layout width == 500 * DSF = 1500
  EXPECT_EQ(1500, constraints.layout_size.width());
  // constraints layout height == 550 * DSF = 1650
  EXPECT_EQ(1650, constraints.layout_size.height());
  EXPECT_NEAR(2.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(2.0f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(2.0f, constraints.maximum_scale, 0.01f);
  EXPECT_FALSE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport3) {
  RegisterMockedHttpURLLoad("viewport/viewport-48.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeWithSettings(SetViewportSettings);
  web_view_impl->MainFrameWidget()->SetDeviceScaleFactorForTesting(3.f);
  frame_test_helpers::LoadFrame(web_view_impl->MainFrameImpl(),
                                base_url_ + "viewport/viewport-48.html");

  Page* page = web_view_helper.GetWebView()->GetPage();
  // Initial width and height must be scaled by DSF.
  PageScaleConstraints constraints = RunViewportTest(page, 960, 1056);

  // constraints layout width == 3000 * DSF = 9000
  EXPECT_EQ(9000, constraints.layout_size.width());
  EXPECT_EQ(1056, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

TEST_F(ViewportTest, viewport4) {
  RegisterMockedHttpURLLoad("viewport/viewport-39.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeWithSettings(SetViewportSettings);
  web_view_impl->MainFrameWidget()->SetDeviceScaleFactorForTesting(3.f);
  frame_test_helpers::LoadFrame(web_view_impl->MainFrameImpl(),
                                base_url_ + "viewport/viewport-39.html");

  Page* page = web_view_helper.GetWebView()->GetPage();
  // Initial width and height must be scaled by DSF.
  PageScaleConstraints constraints = RunViewportTest(page, 960, 1056);

  // constraints layout width == 200 * DSF = 600
  EXPECT_EQ(600, constraints.layout_size.width());
  // constraints layout height == 700 * DSF = 2100
  EXPECT_EQ(2100, constraints.layout_size.height());
  EXPECT_NEAR(1.6f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(1.6f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

// Verifies that the value clamping from
// https://www.w3.org/TR/css-device-adapt-1/#width-and-height-properties
// applies to CSS pixel not physical pixel.
TEST_F(ViewportTest, viewport5) {
  RegisterMockedHttpURLLoad("viewport/viewport-48.html");

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl =
      web_view_helper.InitializeWithSettings(SetViewportSettings);
  web_view_impl->MainFrameWidget()->SetDeviceScaleFactorForTesting(4.f);
  frame_test_helpers::LoadFrame(web_view_impl->MainFrameImpl(),
                                base_url_ + "viewport/viewport-48.html");

  Page* page = web_view_helper.GetWebView()->GetPage();
  // Initial width and height must be scaled by DSF.
  PageScaleConstraints constraints = RunViewportTest(page, 960, 1056);

  // constraints layout width == 3000 * DSF = 12000 and it should not be clamped
  // to 10000.
  EXPECT_EQ(12000, constraints.layout_size.width());
  EXPECT_EQ(1056, constraints.layout_size.height());
  EXPECT_NEAR(1.0f, constraints.initial_scale, 0.01f);
  EXPECT_NEAR(0.25f, constraints.minimum_scale, 0.01f);
  EXPECT_NEAR(5.0f, constraints.maximum_scale, 0.01f);
  EXPECT_TRUE(page->GetViewportDescription().user_zoom);
}

class ViewportHistogramsTest : public SimTest {
 public:
  ViewportHistogramsTest() = default;

  void SetUp() override {
    SimTest::SetUp();

    WebView().GetSettings()->SetViewportEnabled(true);
    WebView().GetSettings()->SetViewportMetaEnabled(true);
    WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 600));
  }

  void UseMetaTag(const String& metaTag) {
    String responseText =
        String("<!DOCTYPE html>") + metaTag +
        String("<style> body { width: 2000px; height: 2000px; } </style>");
    RunTest(responseText);
  }

  void UseDocType(const String& docType) {
    String responseText =
        docType +
        String("<style> body { width: 2000px; height: 2000px; } </style>");
    RunTest(responseText);
  }

  void ExpectType(ViewportDescription::ViewportUMAType type) {
    histogram_tester_.ExpectUniqueSample("Viewport.MetaTagType",
                                         static_cast<int>(type), 1);
  }

  void ExpectTotalCount(const std::string& histogram, int count) {
    histogram_tester_.ExpectTotalCount(histogram, 0);
  }

 private:
  void RunTest(const String& responseText) {
    SimRequest request("https://example.com/test.html", "text/html");
    LoadURL("https://example.com/test.html");
    request.Complete(responseText);

    // Pump the task queue so the meta tag gets processed.
    blink::test::RunPendingTasks();
  }

  base::HistogramTester histogram_tester_;
};

TEST_F(ViewportHistogramsTest, NoOpOnWhenViewportDisabled) {
  WebView().GetSettings()->SetViewportEnabled(false);
  UseMetaTag("<meta name='viewport' content='width=device-width'>");

  ExpectTotalCount("Viewport.MetaTagType", 0);
}

TEST_F(ViewportHistogramsTest, TypeNone) {
  UseMetaTag("");
  ExpectType(ViewportDescription::ViewportUMAType::kNoViewportTag);
}

TEST_F(ViewportHistogramsTest, TypeDeviceWidth) {
  UseMetaTag("<meta name='viewport' content='width=device-width'>");
  ExpectType(ViewportDescription::ViewportUMAType::kDeviceWidth);
}

TEST_F(ViewportHistogramsTest, TypeConstant) {
  UseMetaTag("<meta name='viewport' content='width=800'>");
  ExpectType(ViewportDescription::ViewportUMAType::kConstantWidth);
}

TEST_F(ViewportHistogramsTest, TypeHandheldFriendlyMeta) {
  UseMetaTag("<meta name='HandheldFriendly' content='true'/> ");
  ExpectType(ViewportDescription::ViewportUMAType::kMetaHandheldFriendly);
}

TEST_F(ViewportHistogramsTest, TypeMobileOptimizedMeta) {
  UseMetaTag("<meta name='MobileOptimized' content='320'/> ");
  ExpectType(ViewportDescription::ViewportUMAType::kMetaMobileOptimized);
}

TEST_F(ViewportHistogramsTest, TypeXhtml) {
  UseDocType(
      "<!DOCTYPE html PUBLIC '-//WAPFORUM//DTD XHTML Mobile 1.1//EN' "
      "'http://www.openmobilealliance.org/tech/DTD/xhtml-mobile11.dtd'");
  ExpectType(ViewportDescription::ViewportUMAType::kXhtmlMobileProfile);
}

class ViewportMetaSimTest : public SimTest {
 public:
  ViewportMetaSimTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().GetSettings()->SetViewportEnabled(true);
    WebView().GetSettings()->SetViewportMetaEnabled(true);
    WebView().GetSettings()->SetViewportStyle(
        mojom::blink::ViewportStyle::kMobile);
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }

  void LoadPageWithHTML(const String& html) {
    SimRequest request("https://example.com/test.html", "text/html");
    LoadURL("https://example.com/test.html");
    request.Complete(html);
    blink::test::RunPendingTasks();
  }
};

// Test that the virtual keyboard mode isn't set when a interactive-widget key
// isn't provided.
TEST_F(ViewportMetaSimTest, VirtualKeyboardUnsetWithFlag) {
  // Without a viewport meta tag.
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
  )HTML");
  EXPECT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kUnset);

  // With a viewport meta tag.
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="width=device-width">
  )HTML");
  EXPECT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kUnset);
}

// Test that an invalid value to the interactive-widget property fails to be
// parsed.
TEST_F(ViewportMetaSimTest, VirtualKeyboardParsingEnabledByFlag) {
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="interactive-widget=invalid-value">
  )HTML");

  // Parsing will still fail but now because the value isn't a valid one.
  EXPECT_EQ(ConsoleMessages().front(),
            "The value \"invalid-value\" for key \"interactive-widget\" is "
            "invalid, and has been ignored.");
}

// Test that the resizes-content value is correctly parsed and set on the
// interactive-widget key.
TEST_F(ViewportMetaSimTest, VirtualKeyboardResizesContent) {
  // Blank page to set the default.
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
  )HTML");
  ASSERT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kUnset);

  // Check resizes-content value is set in a basic test case.
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="interactive-widget=resizes-content">
  )HTML");

  EXPECT_TRUE(ConsoleMessages().empty()) << ConsoleMessages().front();
  EXPECT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kResizesContent);

  // Ensure a blank page resets the value.
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
  )HTML");
  EXPECT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kUnset);

  // Mixed with other keys.
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="width=device-width,interactive-widget=resizes-content,minimum-scale=1">
  )HTML");

  EXPECT_TRUE(ConsoleMessages().empty()) << ConsoleMessages().front();
  EXPECT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kResizesContent);
}

// Test that the resizes-visual value is correctly parsed and set on the
// interactive-widget key.
TEST_F(ViewportMetaSimTest, VirtualKeyboardResizeVisual) {
  // Blank page to set the default.
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
  )HTML");
  ASSERT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kUnset);

  // Check resizes-visual value is set.
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="interactive-widget=resizes-visual">
  )HTML");

  EXPECT_TRUE(ConsoleMessages().empty()) << ConsoleMessages().front();
  EXPECT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kResizesVisual);
}

// Test that the overlays-content value is correctly parsed and set on the
// interactive-widget key.
TEST_F(ViewportMetaSimTest, VirtualKeyboardOverlaysContent) {
  // Blank page to set the default.
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
  )HTML");
  ASSERT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kUnset);

  // Check overlays-content value is set.
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="interactive-widget=overlays-content">
  )HTML");

  EXPECT_TRUE(ConsoleMessages().empty()) << ConsoleMessages().front();
  EXPECT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kOverlaysContent);
}

// Test that the virtualKeyboard.overlaysContent API overrides any values set
// from the meta tag and that unsetting it goes back to using the meta tag
// keyboard mode.
TEST_F(ViewportMetaSimTest, VirtualKeyboardAPIOverlaysContent) {
  v8::HandleScope handle_scope(
      WebView().GetPage()->GetAgentGroupScheduler().Isolate());
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="interactive-widget=resizes-content">
  )HTML");

  ASSERT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kResizesContent);

  MainFrame().ExecuteScript(
      WebScriptSource("navigator.virtualKeyboard.overlaysContent = true;"));

  EXPECT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kOverlaysContent);

  MainFrame().ExecuteScript(
      WebScriptSource("navigator.virtualKeyboard.overlaysContent = false;"));

  EXPECT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kResizesContent);
}

// Ensure that updating the content to a bad value causes the mode to become
// unset.
TEST_F(ViewportMetaSimTest, VirtualKeyboardUpdateContent) {
  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="interactive-widget=resizes-content">
  )HTML");

  ASSERT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kResizesContent);

  Element* meta = GetDocument().QuerySelector(AtomicString("[name=viewport]"));
  meta->setAttribute(html_names::kContentAttr,
                     AtomicString("interactive-widget=bad-value"));

  EXPECT_EQ(WebView().VirtualKeyboardModeForTesting(),
            ui::mojom::blink::VirtualKeyboardMode::kUnset);
}

// Test use counters for values of the 'interactive-widget' property.
TEST_F(ViewportMetaSimTest, InteractiveWidgetUseCounters) {
  // Property unset.
  {
    LoadPageWithHTML(R"HTML(
      <!DOCTYPE html>
      <meta name="viewport" content="width=device-width">
    )HTML");

    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetResizesVisual));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetResizesContent));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetOverlaysContent));
  }

  // resizes-visual.
  {
    LoadPageWithHTML(R"HTML(
      <!DOCTYPE html>
      <meta name="viewport" content="interactive-widget=resizes-visual">
    )HTML");

    EXPECT_TRUE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetResizesVisual));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetResizesContent));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetOverlaysContent));
  }

  // resizes-content.
  {
    LoadPageWithHTML(R"HTML(
      <!DOCTYPE html>
      <meta name="viewport" content="interactive-widget=resizes-content">
    )HTML");

    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetResizesVisual));
    EXPECT_TRUE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetResizesContent));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetOverlaysContent));
  }

  // overlays-content.
  {
    LoadPageWithHTML(R"HTML(
      <!DOCTYPE html>
      <meta name="viewport" content="interactive-widget=overlays-content">
    )HTML");

    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetResizesVisual));
    EXPECT_FALSE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetResizesContent));
    EXPECT_TRUE(GetDocument().IsUseCounted(
        WebFeature::kInteractiveWidgetOverlaysContent));
  }
}

// Test that the zoom factor for the device scale is used in the calculation of
// the viewport layout width when browser zoom is applied.
TEST_F(ViewportMetaSimTest, PageZoomDoesntAffectMobileLayoutSize_WidthDefault) {
  const float zoom_factor = 3.f;

  // This will set the device scale zoom factor.
  WebView().MainFrameWidget()->SetDeviceScaleFactorForTesting(zoom_factor);
  // This will set the browser zoom level. This must not affect the layout size.
  WebView().MainFrameWidget()->SetZoomLevelForTesting(1.5f);

  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
  )HTML");

  Compositor().BeginFrame();

  // 980 (default viewport width) * 3 (zoom factor) = 2940.
  EXPECT_EQ(2940, GetDocument().View()->GetLayoutSize().width());
}

TEST_F(ViewportMetaSimTest, PageZoomDoesntAffectMobileLayoutSize_Width1000) {
  const float zoom_factor = 3.f;

  // This will set the device scale zoom factor.
  WebView().MainFrameWidget()->SetDeviceScaleFactorForTesting(zoom_factor);
  // This will set the browser zoom level. This must not affect the layout size.
  WebView().MainFrameWidget()->SetZoomLevelForTesting(1.5f);

  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="width=1000">
  )HTML");

  Compositor().BeginFrame();

  // 1000 (viewport width) * 3 (zoom factor) = 3000.
  EXPECT_EQ(3000, GetDocument().View()->GetLayoutSize().width());
}

TEST_F(ViewportMetaSimTest, PageZoomDoesntAffectMobileLayoutSize_WidthDevice) {
  const float zoom_factor = 3.f;

  // This will set the device scale zoom factor.
  WebView().MainFrameWidget()->SetDeviceScaleFactorForTesting(zoom_factor);
  // This will set the browser zoom level. This must not affect the layout size.
  WebView().MainFrameWidget()->SetZoomLevelForTesting(1.5f);

  LoadPageWithHTML(R"HTML(
    <!DOCTYPE html>
    <meta name="viewport" content="width=device-width">
  )HTML");

  Compositor().BeginFrame();

  // 800 (device width) * 3 (zoom factor) = 2400.
  EXPECT_EQ(2400, GetDocument().View()->GetLayoutSize().width());
}

}  // namespace blink
