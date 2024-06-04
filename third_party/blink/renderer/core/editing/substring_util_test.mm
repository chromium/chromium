// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/substring_util.h"

#include <CoreFoundation/CoreFoundation.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class SubStringUtilTest : public testing::Test {
 public:
  SubStringUtilTest() : base_url_("http://www.test.com/") {}

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  std::string RegisterMockedHttpURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |web_view_helper_|.
    return url_test_helpers::RegisterMockedURLLoadFromBase(
               WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
               WebString::FromUTF8(file_name))
        .GetString()
        .Utf8();
  }

  test::TaskEnvironment task_environment_;

  std::string base_url_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(SubStringUtilTest, SubstringUtil) {
  RegisterMockedHttpURLLoad("content_editable_populated.html");
  WebView* web_view = static_cast<WebView*>(web_view_helper_.InitializeAndLoad(
      base_url_ + "content_editable_populated.html"));

  web_view->GetSettings()->SetDefaultFontSize(12);
  web_view->MainFrameWidget()->Resize(gfx::Size(400, 400));
  WebLocalFrameImpl* frame =
      static_cast<WebLocalFrameImpl*>(web_view->MainFrame());

  gfx::Point baseline_point;
  base::apple::ScopedCFTypeRef<CFAttributedStringRef> result =
      SubstringUtil::AttributedSubstringInRange(frame->GetFrame(), 10, 3,
                                                baseline_point);
  ASSERT_TRUE(result);

  gfx::Point point(baseline_point);
  result.reset(SubstringUtil::AttributedWordAtPoint(frame->FrameWidgetImpl(),
                                                    point, baseline_point));
  ASSERT_TRUE(result);

  web_view->MainFrameWidget()->SetZoomLevel(3);

  result.reset(SubstringUtil::AttributedSubstringInRange(frame->GetFrame(), 5,
                                                         5, baseline_point));
  ASSERT_TRUE(result);

  point = baseline_point;
  result.reset(SubstringUtil::AttributedWordAtPoint(frame->FrameWidgetImpl(),
                                                    point, baseline_point));
  ASSERT_TRUE(result);
}

TEST_F(SubStringUtilTest, SubstringUtilBaselinePoint) {
  RegisterMockedHttpURLLoad("content_editable_multiline.html");
  WebView* web_view = static_cast<WebView*>(web_view_helper_.InitializeAndLoad(
      base_url_ + "content_editable_multiline.html"));
  web_view->GetSettings()->SetDefaultFontSize(12);
  web_view->MainFrameWidget()->Resize(gfx::Size(400, 400));
  WebLocalFrameImpl* frame =
      static_cast<WebLocalFrameImpl*>(web_view->MainFrame());

  gfx::Point old_point;
  SubstringUtil::AttributedSubstringInRange(frame->GetFrame(), 3, 1, old_point);

  gfx::Point new_point;
  SubstringUtil::AttributedSubstringInRange(frame->GetFrame(), 3, 20,
                                            new_point);

  EXPECT_EQ(old_point.x(), new_point.x());
  EXPECT_EQ(old_point.y(), new_point.y());
}

TEST_F(SubStringUtilTest, SubstringUtilPinchZoom) {
  RegisterMockedHttpURLLoad("content_editable_populated.html");
  WebView* web_view = static_cast<WebView*>(web_view_helper_.InitializeAndLoad(
      base_url_ + "content_editable_populated.html"));
  web_view->GetSettings()->SetDefaultFontSize(12);
  web_view->MainFrameWidget()->Resize(gfx::Size(400, 400));
  WebLocalFrameImpl* frame =
      static_cast<WebLocalFrameImpl*>(web_view->MainFrame());

  gfx::Point baseline_point;
  base::apple::ScopedCFTypeRef<CFAttributedStringRef> result =
      SubstringUtil::AttributedSubstringInRange(frame->GetFrame(), 10, 3,
                                                baseline_point);
  ASSERT_TRUE(result);

  web_view->SetPageScaleFactor(3);

  gfx::Point point_after_zoom;
  result.reset(SubstringUtil::AttributedSubstringInRange(frame->GetFrame(), 10,
                                                         3, point_after_zoom));
  ASSERT_TRUE(result);

  // We won't have moved by a full factor of 3 because of the translations, but
  // we should move by a factor of >2.
  EXPECT_LT(2 * baseline_point.x(), point_after_zoom.x());
  EXPECT_LT(2 * baseline_point.y(), point_after_zoom.y());
}

TEST_F(SubStringUtilTest, SubstringUtilIframe) {
  RegisterMockedHttpURLLoad("single_iframe.html");
  RegisterMockedHttpURLLoad("visible_iframe.html");
  WebView* web_view = static_cast<WebView*>(
      web_view_helper_.InitializeAndLoad(base_url_ + "single_iframe.html"));
  web_view->GetSettings()->SetDefaultFontSize(12);
  web_view->GetSettings()->SetJavaScriptEnabled(true);
  web_view->MainFrameWidget()->Resize(gfx::Size(400, 400));
  WebLocalFrameImpl* main_frame =
      static_cast<WebLocalFrameImpl*>(web_view->MainFrame());
  WebLocalFrameImpl* child_frame = WebLocalFrameImpl::FromFrame(
      To<LocalFrame>(main_frame->GetFrame()->Tree().FirstChild()));

  gfx::Point baseline_point;
  base::apple::ScopedCFTypeRef<CFAttributedStringRef> result =
      SubstringUtil::AttributedSubstringInRange(child_frame->GetFrame(), 11, 7,
                                                baseline_point);
  ASSERT_TRUE(result);

  gfx::Point point(baseline_point);
  result.reset(SubstringUtil::AttributedWordAtPoint(
      main_frame->FrameWidgetImpl(), point, baseline_point));
  ASSERT_TRUE(result);

  int y_before_change = baseline_point.y();

  // Now move the <iframe> down by 100px.
  main_frame->ExecuteScript(WebScriptSource(
      "document.querySelector('iframe').style.marginTop = '100px';"));

  point = gfx::Point(point.x(), point.y() + 100);
  result.reset(SubstringUtil::AttributedWordAtPoint(
      main_frame->FrameWidgetImpl(), point, baseline_point));
  ASSERT_TRUE(result);

  EXPECT_EQ(y_before_change, baseline_point.y() - 100);
}

}  // namespace blink
