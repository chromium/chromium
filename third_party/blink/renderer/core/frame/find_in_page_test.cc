// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/find_in_page.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using blink::test::RunPendingTasks;

namespace blink {

class FindInPageTest : public testing::Test {
 protected:
  FindInPageTest() {
    web_view_helper_.Initialize();
    WebLocalFrameImpl& frame_impl = *web_view_helper_.LocalMainFrame();
    document_ = static_cast<Document*>(frame_impl.GetDocument());
    find_in_page_ = frame_impl.GetFindInPage();
  }

  void SetUp() override {
    web_view_helper_.Resize(gfx::Size(640, 480));
    web_view_helper_.GetWebView()->MainFrameWidget()->SetFocus(true);
    test::RunPendingTasks();
  }

  Document& GetDocument() const;
  FindInPage& GetFindInPage() const;
  TextFinder& GetTextFinder() const;

 private:
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  Persistent<Document> document_;
  Persistent<FindInPage> find_in_page_;
};

Document& FindInPageTest::GetDocument() const {
  return *document_;
}

FindInPage& FindInPageTest::GetFindInPage() const {
  return *find_in_page_;
}

TextFinder& FindInPageTest::GetTextFinder() const {
  return find_in_page_->EnsureTextFinder();
}

class FindInPageCallbackReceiver {
 public:
  FindInPageCallbackReceiver() { is_called = false; }

  bool IsCalled() { return is_called; }

  void AssertFindMatchRects(int expected_version,
                            const WebVector<gfx::RectF>& expected_rects,
                            const gfx::RectF& expected_active_match_rect,
                            int actual_version,
                            const Vector<gfx::RectF>& actual_rects,
                            const gfx::RectF& actual_active_match_rect) {
    is_called = true;
    EXPECT_EQ(expected_version, actual_version);
    EXPECT_EQ(expected_rects.size(), actual_rects.size());
    EXPECT_EQ(expected_active_match_rect, actual_active_match_rect);
    for (wtf_size_t i = 0; i < actual_rects.size(); ++i) {
      EXPECT_EQ(expected_rects[i], actual_rects[i]);
    }
  }

 private:
  bool is_called;
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(FindInPageTest, FindMatchRectsReturnsCorrectRects) {
  GetDocument().body()->setInnerHTML("aAaAbBaBbAaAaA");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  WebString search_text(String("aA"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  int rects_version = GetTextFinder().FindMatchMarkersVersion();
  FindInPageCallbackReceiver callback_receiver;
  GetFindInPage().FindMatchRects(
      rects_version - 1,
      WTF::BindOnce(&FindInPageCallbackReceiver::AssertFindMatchRects,
                    WTF::Unretained(&callback_receiver), rects_version,
                    GetTextFinder().FindMatchRects(),
                    GetTextFinder().ActiveFindMatchRect()));
  EXPECT_TRUE(callback_receiver.IsCalled());
}
#endif

TEST_F(FindInPageTest, FindAllAs) {
  std::ostringstream str;
  for (int i = 0; i < 10'000; ++i)
    str << "a ";

  GetDocument().body()->setInnerHTML(str.str().c_str());
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  WebString search_text(String("a"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);
  EXPECT_EQ(10'000, GetTextFinder().TotalMatchCount());
}

}  // namespace blink
