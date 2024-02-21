// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_disallow_transition_scope.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

#if DCHECK_IS_ON()

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"

namespace blink {

using blink::frame_test_helpers::WebViewHelper;

class WebDisallowTransitionScopeTest : public testing::Test {
 protected:
  Document* TopDocument() const;
  WebDocument TopWebDocument() const;

  test::TaskEnvironment task_environment_;
  WebViewHelper web_view_helper_;
};

Document* WebDisallowTransitionScopeTest::TopDocument() const {
  return To<LocalFrame>(web_view_helper_.GetWebView()->GetPage()->MainFrame())
      ->GetDocument();
}

WebDocument WebDisallowTransitionScopeTest::TopWebDocument() const {
  return web_view_helper_.LocalMainFrame()->GetDocument();
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/1067036): the death test fails on Android.
TEST_F(WebDisallowTransitionScopeTest, TestDisallowTransition) {
  // Make the death test thread-safe. For more info, see:
  // https://github.com/google/googletest/blob/main/googletest/docs/advanced.md#death-tests-and-threads
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  web_view_helper_.InitializeAndLoad("about:blank");

  WebDocument web_doc = TopWebDocument();
  Document* core_doc = TopDocument();

  // Legal transition.
  core_doc->Lifecycle().AdvanceTo(DocumentLifecycle::kLayoutClean);

  {
    // Illegal transition.
    WebDisallowTransitionScope disallow(&web_doc);
    EXPECT_DEATH_IF_SUPPORTED(core_doc->Lifecycle().EnsureStateAtMost(
                                  DocumentLifecycle::kVisualUpdatePending),
                              "Cannot rewind document lifecycle");
  }

  // Legal transition.
  core_doc->Lifecycle().EnsureStateAtMost(
      DocumentLifecycle::kVisualUpdatePending);
}
#endif

}  // namespace blink

#endif  // DCHECK_IS_ON()
