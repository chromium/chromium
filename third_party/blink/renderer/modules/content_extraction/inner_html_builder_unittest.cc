// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/inner_html_builder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace {

TEST(InnerHtmlBuilderTest, Basic) {
  test::TaskEnvironment task_environment;
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();
  ASSERT_TRUE(helper.LocalMainFrame());
  frame_test_helpers::LoadHTMLString(
      helper.LocalMainFrame(),
      "<body>container<iframe></iframe><script>let x = 10;</script>X</body>",
      url_test_helpers::ToKURL("http://foobar.com"));
  EXPECT_EQ("<body>container<iframe></iframe>X</body>",
            InnerHtmlBuilder::Build(*helper.LocalMainFrame()->GetFrame()));
}
}  // namespace
}  // namespace blink
