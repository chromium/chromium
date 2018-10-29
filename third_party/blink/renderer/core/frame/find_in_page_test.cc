// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/find_in_page.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
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

  Document& GetDocument() const;
  FindInPage& GetFindInPage() const;
  TextFinder& GetTextFinder() const;

 private:
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
                            const WebVector<WebFloatRect>& expected_rects,
                            const WebFloatRect& expected_active_match_rect,
                            int actual_version,
                            const Vector<WebFloatRect>& actual_rects,
                            const WebFloatRect& actual_active_match_rect) {
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

TEST_F(FindInPageTest, FindMatchRectsReturnsCorrectRects) {
  GetDocument().body()->SetInnerHTMLFromString("aAaAbBaBbAaAaA");
  GetDocument().UpdateStyleAndLayout();

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
      base::BindOnce(&FindInPageCallbackReceiver::AssertFindMatchRects,
                     base::Unretained(&callback_receiver), rects_version,
                     GetTextFinder().FindMatchRects(),
                     GetTextFinder().ActiveFindMatchRect()));
  EXPECT_TRUE(callback_receiver.IsCalled());
}

}  // namespace blink
