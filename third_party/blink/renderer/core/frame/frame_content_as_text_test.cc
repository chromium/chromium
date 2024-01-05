// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_content_as_text.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class FrameContentAsTextTest : public testing::Test {
 public:
  FrameContentAsTextTest() = default;
  ~FrameContentAsTextTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void RegisterMockedHttpURLLoad(const std::string& file_path) {
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_path));
  }

 protected:
  const std::string base_url_ = "http://test.com/";

 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(FrameContentAsTextTest, RenderedDocumentsOnly) {
  frame_test_helpers::WebViewHelper web_view_helper;

  RegisterMockedHttpURLLoad("display_none_frame.html");

  WebView* web_view =
      web_view_helper.InitializeAndLoad(base_url_ + "display_none_frame.html");

  StringBuilder text;

  WebLocalFrame* local_frame = web_view->MainFrame()->ToWebLocalFrame();

  FrameContentAsText(
      /*max_chars=*/100, To<WebLocalFrameImpl>(local_frame)->GetFrame(), text);

  EXPECT_EQ(String(""), text.ToString());
}

}  // namespace blink
