// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_manager_impl.h"

#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#import "ios/web/find_in_page/find_in_page_constants.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_find_in_page_manager_delegate.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Frame ids of fake web frames used in this test class.
const char kOneMatchFrameId[] = "frame_with_one_match";
const char kTwoMatchesFrameId[] = "frame_with_two_matches";

}  // namespace

namespace web {

// Tests FindInPageManagerImpl and verifies that the state of
// FindInPageManagerDelegate is correct depending on what web frames return.
class FindInPageManagerImplTest : public WebTest {
 protected:
  FindInPageManagerImplTest() {
    test_web_state_ = std::make_unique<TestWebState>();
    auto frames_manager = std::make_unique<FakeWebFramesManager>();
    fake_web_frames_manager_ = frames_manager.get();
    test_web_state_->SetWebFramesManager(std::move(frames_manager));
    FindInPageManagerImpl::CreateForWebState(test_web_state_.get());
    GetFindInPageManager()->SetDelegate(&fake_delegate_);
  }

  // Returns the FindInPageManager associated with |test_web_state_|.
  FindInPageManager* GetFindInPageManager() {
    return FindInPageManager::FromWebState(test_web_state_.get());
  }
  // Returns a FakeWebFrame that is the main frame with id |frame_id| that will
  // return |js_result| for the JavaScript function call
  // "findInString.findString".
  std::unique_ptr<FakeWebFrame> CreateMainWebFrameWithJsResultForFind(
      std::unique_ptr<base::Value> js_result,
      const std::string& frame_id) {
    return CreateWebFrameWithJsResultForFind(std::move(js_result), frame_id,
                                             /*is_main_frame=*/true);
  }
  // Returns a FakeWebFrame child frame with id |frame_id| that
  // will return |js_result| for the JavaScript function call
  // "findInString.findString".
  std::unique_ptr<FakeWebFrame> CreateChildWebFrameWithJsResultForFind(
      std::unique_ptr<base::Value> js_result,
      const std::string& frame_id) {
    return CreateWebFrameWithJsResultForFind(std::move(js_result), frame_id,
                                             /*is_main_frame=*/false);
  }
  // Returns a FakeWebFrame with id |frame_id| that will return |js_result| for
  // the JavaScript function call "findInString.findString".
  std::unique_ptr<FakeWebFrame> CreateWebFrameWithJsResultForFind(
      std::unique_ptr<base::Value> js_result,
      const std::string& frame_id,
      bool is_main_frame) {
    auto frame =
        std::make_unique<FakeWebFrame>(frame_id, is_main_frame, GURL());
    frame->AddJsResultForFunctionCall(std::move(js_result), kFindInPageSearch);

    return frame;
  }

  void AddWebFrame(std::unique_ptr<WebFrame> frame) {
    WebFrame* frame_ptr = frame.get();
    fake_web_frames_manager_->AddWebFrame(std::move(frame));
    test_web_state_->OnWebFrameDidBecomeAvailable(frame_ptr);
  }

  void RemoveWebFrame(const std::string& frame_id) {
    WebFrame* frame_ptr = fake_web_frames_manager_->GetFrameWithId(frame_id);
    test_web_state_->OnWebFrameWillBecomeUnavailable(frame_ptr);
    fake_web_frames_manager_->RemoveWebFrame(frame_id);
  }

  std::unique_ptr<TestWebState> test_web_state_;
  FakeWebFramesManager* fake_web_frames_manager_;
  FakeFindInPageManagerDelegate fake_delegate_;
};

// Tests that Find In Page responds with a total match count of three when a
// frame has one match and another frame has two matches.
TEST_F(FindInPageManagerImplTest, FindMatchesMultipleFrames) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches = CreateChildWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(2ul, frame_with_one_match_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());
  EXPECT_EQ(3, fake_delegate_.state()->match_count);
}

// Tests that Find In Page responds with a total match count of one when a frame
// has one match but find in one frame was cancelled. This can occur if the
// frame becomes unavailable.
TEST_F(FindInPageManagerImplTest, FrameCancelFind) {
  auto frame_with_null_result = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(), "frame");
  FakeWebFrame* frame_with_null_result_ptr = frame_with_null_result.get();
  auto frame_with_one_match = CreateChildWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  AddWebFrame(std::move(frame_with_null_result));
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_null_result_ptr->GetLastJavaScriptCall());
  ASSERT_EQ(2ul, frame_with_one_match_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(1, fake_delegate_.state()->match_count);
}

// Tests that Find In Page returns a total match count matching the latest find
// if two finds are called.
TEST_F(FindInPageManagerImplTest, ReturnLatestFind) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  auto frame_with_two_matches = CreateChildWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
  RemoveWebFrame(kOneMatchFrameId);
  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(3ul, frame_with_two_matches_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[2]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(2, fake_delegate_.state()->match_count);
}

// Tests that Find In Page should not return if the web state is destroyed
// during a find.
TEST_F(FindInPageManagerImplTest, DestroyWebStateDuringFind) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  test_web_state_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_delegate_.state());
}

// Tests that Find In Page updates total match count when a frame with matches
// becomes unavailable during find.
TEST_F(FindInPageManagerImplTest, FrameUnavailableAfterDelegateCallback) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches = CreateChildWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  RemoveWebFrame(kTwoMatchesFrameId);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(2ul, frame_with_one_match_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(1, fake_delegate_.state()->match_count);
}

// Tests that Find In Page returns with the right match count for a frame with
// one match and another that requires pumping to return its two matches.
TEST_F(FindInPageManagerImplTest, FrameRespondsWithPending) {
  std::unique_ptr<FakeWebFrame> frame_with_two_matches =
      CreateMainWebFrameWithJsResultForFind(std::make_unique<base::Value>(-1.0),
                                            kTwoMatchesFrameId);
  frame_with_two_matches->AddJsResultForFunctionCall(
      std::make_unique<base::Value>(2.0), kFindInPagePump);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_two_matches));
  auto frame_with_one_match = CreateChildWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(3ul, frame_with_two_matches_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[2]);
  EXPECT_EQ("__gCrWeb.findInPage.pumpSearch(100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());
  EXPECT_EQ(3, fake_delegate_.state()->match_count);
}

// Tests that Find In Page doesn't fail when delegate is not set.
TEST_F(FindInPageManagerImplTest, DelegateNotSet) {
  GetFindInPageManager()->SetDelegate(nullptr);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());
  base::RunLoop().RunUntilIdle();
}

// Tests that Find In Page returns no matches if can't call JavaScript function.
TEST_F(FindInPageManagerImplTest, FrameCannotCallJavaScriptFunction) {
  auto frame_cannot_call_func = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  frame_cannot_call_func->set_can_call_function(false);
  AddWebFrame(std::move(frame_cannot_call_func));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->match_count);
}

// Tests that  Find In Page responds with a total match count of zero when there
// are no known webpage frames.
TEST_F(FindInPageManagerImplTest, NoFrames) {
  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->match_count);
}

// Tests that Find in Page responds with a total match count of zero when there
// are no matches in the only frame. Tests that Find in Page also did not
// respond with an selected match index value.
TEST_F(FindInPageManagerImplTest, FrameWithNoMatchNoHighlight) {
  auto frame_with_zero_matches = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(0.0), "frame_with_zero_matches");
  FakeWebFrame* frame_with_zero_matches_ptr = frame_with_zero_matches.get();
  AddWebFrame(std::move(frame_with_zero_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(1ul,
            frame_with_zero_matches_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_zero_matches_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(0, fake_delegate_.state()->match_count);
  EXPECT_EQ(-1, fake_delegate_.state()->index);
}

// Tests that Find in Page responds with index zero after a find when there are
// two matches in a frame.
TEST_F(FindInPageManagerImplTest, DidHighlightFirstIndex) {
  auto frame_with_two_matches = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(2ul, frame_with_two_matches_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(0, fake_delegate_.state()->index);
}

// Tests that Find in Page responds with index one to a FindInPageNext find
// after a FindInPageSearch find finishes when there are two matches in a frame.
TEST_F(FindInPageManagerImplTest, FindDidHighlightSecondIndexAfterNextCall) {
  auto frame_with_two_matches = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(2ul, frame_with_two_matches_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[0]);

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state()->index > -1;
  }));
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(1);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());
  EXPECT_EQ(1, fake_delegate_.state()->index);
}

// Tests that Find in Page selects all matches in a page with one frame with one
// match and another with two matches when making successive FindInPageNext
// calls.
TEST_F(FindInPageManagerImplTest, FindDidSelectAllMatchesWithNextCall) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches = CreateChildWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->index);
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(1, fake_delegate_.state()->index);
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(2, fake_delegate_.state()->index);
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(1);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->index);
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());
}

// Tests that Find in Page selects all matches in a page with one frame with one
// match and another with two matches when making successive FindInPagePrevious
// calls.
TEST_F(FindInPageManagerImplTest,
       FindDidLoopThroughAllMatchesWithPreviousCall) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches = CreateChildWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->index);
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(2, fake_delegate_.state()->index);
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(1);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(1, fake_delegate_.state()->index);
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->index);
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());
}

// Tests that Find in Page responds with index two to a FindInPagePrevious find
// after a FindInPageSearch find finishes when there are two matches in a
// frame and one match in another.
TEST_F(FindInPageManagerImplTest, FindDidHighlightLastIndexAfterPreviousCall) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches = CreateChildWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());
  ASSERT_EQ(2ul, frame_with_one_match_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ("__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[0]);

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state()->index == 2;
  }));
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(1);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());
}

// Tests that Find in Page does not respond to a FindInPageNext or a
// FindInPagePrevious call if no FindInPageSearch find was executed beforehand.
TEST_F(FindInPageManagerImplTest, FindDidNotRepondToNextOrPrevIfNoSearch) {
  auto frame_with_three_matches = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(3.0), "frame_with_three_matches");
  AddWebFrame(std::move(frame_with_three_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_delegate_.state());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_delegate_.state());
}

// Tests that Find in Page responds with index one for a successive
// FindInPageNext after the frame containing the currently selected match is
// removed.
TEST_F(FindInPageManagerImplTest,
       FindDidHighlightNextMatchAfterFrameDisappears) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches = CreateChildWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(2ul, frame_with_one_match_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[1]);

  RemoveWebFrame(kOneMatchFrameId);
  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state()->index == 0;
  }));
  EXPECT_EQ("__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());
}

// Tests that Find in Page does not respond when frame is removed
TEST_F(FindInPageManagerImplTest, FindDidNotRepondAfterFrameRemoved) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  AddWebFrame(std::move(frame_with_one_match));

  RemoveWebFrame(kOneMatchFrameId);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_delegate_.state());
}

// Tests that Find in Page responds with a total match count of one to a
// FindInPageSearch find when there is one match in a frame and then responds
// with a total match count of zero when that frame is removed.
TEST_F(FindInPageManagerImplTest, FindInPageUpdateMatchCountAfterFrameRemoved) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));

  RemoveWebFrame(kOneMatchFrameId);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state()->match_count == 0;
  }));
}

// Tests that DidHighlightMatches is not called when a frame with no matches is
// removed from the page.
TEST_F(FindInPageManagerImplTest, FindDidNotResponseAfterFrameDisappears) {
  const char kZeroMatchesFrameId[] = "frame_with_zero_matches";
  auto frame_with_zero_matches = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(0.0), kZeroMatchesFrameId);
  auto frame_with_two_matches = CreateChildWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), kTwoMatchesFrameId);
  AddWebFrame(std::move(frame_with_zero_matches));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));

  fake_delegate_.Reset();
  RemoveWebFrame(kZeroMatchesFrameId);

  EXPECT_FALSE(fake_delegate_.state());
}

// Tests that Find in Page SetContentIsHTML() returns true if the web state's
// content is HTML and returns false if the web state's content is not HTML.
TEST_F(FindInPageManagerImplTest, FindInPageCanSearchContent) {
  test_web_state_->SetContentIsHTML(false);

  EXPECT_FALSE(GetFindInPageManager()->CanSearchContent());

  test_web_state_->SetContentIsHTML(true);

  EXPECT_TRUE(GetFindInPageManager()->CanSearchContent());
}

// Tests that Find in Page resets the match count to 0 and the query to nil
// after calling StopFinding().
TEST_F(FindInPageManagerImplTest, FindInPageCanStopFind) {
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(1.0), kOneMatchFrameId);
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state() && fake_delegate_.state()->match_count == 1;
  }));

  GetFindInPageManager()->StopFinding();
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state() && fake_delegate_.state()->match_count == 0;
  }));
  EXPECT_FALSE(fake_delegate_.state()->query);
}

// Tests that Find in Page responds with an updated match count when calling
// FindInPageNext after the visible match count in a frame changes following a
// FindInPageSearch. This simulates a once hidden match becoming visible between
// a FindInPageSearch and a FindInPageNext.
TEST_F(FindInPageManagerImplTest, FindInPageNextUpdatesMatchCount) {
  auto frame_with_hidden_match = CreateMainWebFrameWithJsResultForFind(
      std::make_unique<base::Value>(2.0), "frame_with_hidden_match");
  FakeWebFrame* frame_with_hidden_match_ptr = frame_with_hidden_match.get();
  AddWebFrame(std::move(frame_with_hidden_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state() && fake_delegate_.state()->match_count == 2;
  }));
  auto select_and_scroll_result = std::make_unique<base::DictionaryValue>();
  select_and_scroll_result->SetDouble("matches", 3.0);
  select_and_scroll_result->SetDouble("index", 1.0);
  frame_with_hidden_match_ptr->AddJsResultForFunctionCall(
      std::move(select_and_scroll_result), kFindInPageSelectAndScrollToMatch);

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state() && fake_delegate_.state()->match_count == 3;
  }));
  EXPECT_EQ(1, fake_delegate_.state()->index);
}

}  // namespace web
