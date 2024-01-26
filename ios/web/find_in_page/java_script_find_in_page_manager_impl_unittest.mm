// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/java_script_find_in_page_manager_impl.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/values.h"
#import "ios/web/find_in_page/find_in_page_constants.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_find_in_page_manager_delegate.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gtest/include/gtest/gtest.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

// Tests JavaScriptFindInPageManagerImpl and verifies that the state of
// FindInPageManagerDelegate is correct depending on what web frames return.
class JavaScriptFindInPageManagerImplTest : public WebTest {
 protected:
  JavaScriptFindInPageManagerImplTest()
      : WebTest(std::make_unique<FakeWebClient>()) {}

  void SetUp() override {
    WebTest::SetUp();

    FindInPageJavaScriptFeature* feature =
        FindInPageJavaScriptFeature::GetInstance();

    fake_web_state_ = std::make_unique<FakeWebState>();
    fake_web_state_->SetBrowserState(GetBrowserState());
    auto frames_manager = std::make_unique<FakeWebFramesManager>();
    fake_web_frames_manager_ = frames_manager.get();
    fake_web_state_->SetWebFramesManager(feature->GetSupportedContentWorld(),
                                         std::move(frames_manager));

    JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
        ->ConfigureFeatures({feature});
    JavaScriptFindInPageManager::CreateForWebState(fake_web_state_.get());
    GetFindInPageManager()->SetDelegate(&fake_delegate_);
  }

  // Returns the JavaScriptFindInPageManager associated with `fake_web_state_`.
  JavaScriptFindInPageManager* GetFindInPageManager() {
    return JavaScriptFindInPageManager::FromWebState(fake_web_state_.get());
  }

  // Returns a fake WebFrame that represents the main frame which will return
  // `js_result` for the JavaScript function call "findInString.findString".
  std::unique_ptr<FakeWebFrame> CreateMainWebFrameWithJsResultForFind(
      base::Value* js_result) {
    auto frame = FakeWebFrame::CreateMainWebFrame(GURL());
    frame->AddJsResultForFunctionCall(js_result, kFindInPageSearch);
    frame->set_browser_state(GetBrowserState());
    return frame;
  }

  // Returns a fake WebFrame that represents a child frame which will return
  // `js_result` for the JavaScript function call "findInString.findString".
  std::unique_ptr<FakeWebFrame> CreateChildWebFrameWithJsResultForFind(
      base::Value* js_result) {
    auto frame = FakeWebFrame::CreateChildWebFrame(GURL());
    frame->AddJsResultForFunctionCall(js_result, kFindInPageSearch);
    frame->set_browser_state(GetBrowserState());
    return frame;
  }

  void AddWebFrame(std::unique_ptr<FakeWebFrame> frame) {
    fake_web_frames_manager_->AddWebFrame(std::move(frame));
  }

  void RemoveWebFrame(const std::string& frame_id) {
    fake_web_frames_manager_->RemoveWebFrame(frame_id);
  }

  std::unique_ptr<FakeWebState> fake_web_state_;
  raw_ptr<FakeWebFramesManager> fake_web_frames_manager_;
  FakeFindInPageManagerDelegate fake_delegate_;
  base::UserActionTester user_action_tester_;
};

// Tests that Find In Page responds with a total match count of three when a
// frame has one match and another frame has two matches.
TEST_F(JavaScriptFindInPageManagerImplTest, FindMatchesMultipleFrames) {
  auto one = std::make_unique<base::Value>(1.0);
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches =
      CreateChildWebFrameWithJsResultForFind(two.get());
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(2ul, frame_with_one_match_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());
  EXPECT_EQ(3, fake_delegate_.state()->match_count);
}

// Tests that Find In Page responds with a total match count of one when a frame
// has one match but find in one frame was cancelled. This can occur if the
// frame becomes unavailable.
TEST_F(JavaScriptFindInPageManagerImplTest, FrameCancelFind) {
  auto null = std::make_unique<base::Value>();
  auto one = std::make_unique<base::Value>(1.0);
  auto frame_with_null_result =
      CreateMainWebFrameWithJsResultForFind(null.get());
  FakeWebFrame* frame_with_null_result_ptr = frame_with_null_result.get();
  auto frame_with_one_match = CreateChildWebFrameWithJsResultForFind(one.get());
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  AddWebFrame(std::move(frame_with_null_result));
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_null_result_ptr->GetLastJavaScriptCall());
  ASSERT_EQ(2ul, frame_with_one_match_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(1, fake_delegate_.state()->match_count);
}

// Tests that Find In Page returns a total match count matching the latest find
// if two finds are called.
TEST_F(JavaScriptFindInPageManagerImplTest, ReturnLatestFind) {
  auto one = std::make_unique<base::Value>(1.0);
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  auto frame_with_two_matches =
      CreateChildWebFrameWithJsResultForFind(two.get());
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  fake_delegate_.Reset();

  RemoveWebFrame(kMainFakeFrameId);
  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(3ul, frame_with_two_matches_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[2]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(2, fake_delegate_.state()->match_count);
}

// Tests that Find In Page should not return if the web state is destroyed
// during a find.
TEST_F(JavaScriptFindInPageManagerImplTest, DestroyWebStateDuringFind) {
  auto one = std::make_unique<base::Value>(1.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  fake_web_state_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_delegate_.state());
}

// Tests that Find In Page updates total match count when a frame with matches
// becomes unavailable during find.
TEST_F(JavaScriptFindInPageManagerImplTest,
       FrameUnavailableAfterDelegateCallback) {
  auto one = std::make_unique<base::Value>(1.0);
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches =
      CreateChildWebFrameWithJsResultForFind(two.get());
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  fake_delegate_.Reset();

  RemoveWebFrame(kChildFakeFrameId);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));

  ASSERT_EQ(2ul, frame_with_one_match_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(1, fake_delegate_.state()->match_count);
}

// Tests that Find In Page returns with the right match count for a frame with
// one match and another that requires pumping to return its two matches.
TEST_F(JavaScriptFindInPageManagerImplTest, FrameRespondsWithPending) {
  auto negative_one = std::make_unique<base::Value>(-1.0);
  auto one = std::make_unique<base::Value>(1.0);
  auto two = std::make_unique<base::Value>(2.0);

  std::unique_ptr<FakeWebFrame> frame_with_two_matches =
      CreateMainWebFrameWithJsResultForFind(negative_one.get());
  frame_with_two_matches->AddJsResultForFunctionCall(two.get(),
                                                     kFindInPagePump);
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_two_matches));
  auto frame_with_one_match = CreateChildWebFrameWithJsResultForFind(one.get());
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(3ul, frame_with_two_matches_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[2]);
  EXPECT_EQ(u"__gCrWeb.findInPage.pumpSearch(100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());
  EXPECT_EQ(3, fake_delegate_.state()->match_count);
}

// Tests that Find In Page doesn't fail when delegate is not set.
TEST_F(JavaScriptFindInPageManagerImplTest, DelegateNotSet) {
  GetFindInPageManager()->SetDelegate(nullptr);
  auto one = std::make_unique<base::Value>(1.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());
  base::RunLoop().RunUntilIdle();
}

// Tests that  Find In Page responds with a total match count of zero when there
// are no known webpage frames.
TEST_F(JavaScriptFindInPageManagerImplTest, NoFrames) {
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
TEST_F(JavaScriptFindInPageManagerImplTest, FrameWithNoMatchNoHighlight) {
  auto zero = std::make_unique<base::Value>(0.0);
  auto frame_with_zero_matches =
      CreateMainWebFrameWithJsResultForFind(zero.get());
  FakeWebFrame* frame_with_zero_matches_ptr = frame_with_zero_matches.get();
  AddWebFrame(std::move(frame_with_zero_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(1ul,
            frame_with_zero_matches_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_zero_matches_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(0, fake_delegate_.state()->match_count);
  EXPECT_EQ(-1, fake_delegate_.state()->index);
}

// Tests that Find in Page responds with index zero after a find when there are
// two matches in a frame.
TEST_F(JavaScriptFindInPageManagerImplTest, DidHighlightFirstIndex) {
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_two_matches =
      CreateMainWebFrameWithJsResultForFind(two.get());
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(2ul, frame_with_two_matches_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[0]);
  EXPECT_EQ(0, fake_delegate_.state()->index);
}

// Tests that Find in Page responds with index one to a FindInPageNext find
// after a FindInPageSearch find finishes when there are two matches in a frame.
TEST_F(JavaScriptFindInPageManagerImplTest,
       FindDidHighlightSecondIndexAfterNextCall) {
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_two_matches =
      CreateMainWebFrameWithJsResultForFind(two.get());
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(2ul, frame_with_two_matches_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetJavaScriptCallHistory()[0]);

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state()->index > -1;
  }));
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(1);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());
  EXPECT_EQ(1, fake_delegate_.state()->index);
}

// Tests that Find in Page selects all matches in a page with one frame with one
// match and another with two matches when making successive FindInPageNext
// calls.
TEST_F(JavaScriptFindInPageManagerImplTest,
       FindDidSelectAllMatchesWithNextCall) {
  auto one = std::make_unique<base::Value>(1.0);
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches =
      CreateChildWebFrameWithJsResultForFind(two.get());
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->index);
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(1, fake_delegate_.state()->index);
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(2, fake_delegate_.state()->index);
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(1);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->index);
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());
}

// Tests that Find in Page selects all matches in a page with one frame with one
// match and another with two matches when making successive FindInPagePrevious
// calls.
TEST_F(JavaScriptFindInPageManagerImplTest,
       FindDidLoopThroughAllMatchesWithPreviousCall) {
  auto one = std::make_unique<base::Value>(1.0);
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches =
      CreateChildWebFrameWithJsResultForFind(two.get());
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->index);
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(2, fake_delegate_.state()->index);
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(1);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(1, fake_delegate_.state()->index);
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(0, fake_delegate_.state()->index);
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetLastJavaScriptCall());
}

// Tests that Find in Page responds with index two to a FindInPagePrevious find
// after a FindInPageSearch find finishes when there are two matches in a
// frame and one match in another.
TEST_F(JavaScriptFindInPageManagerImplTest,
       FindDidHighlightLastIndexAfterPreviousCall) {
  auto one = std::make_unique<base::Value>(1.0);
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches =
      CreateChildWebFrameWithJsResultForFind(two.get());
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());
  ASSERT_EQ(2ul, frame_with_one_match_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[1]);
  EXPECT_EQ(u"__gCrWeb.findInPage.findString(\"foo\", 100.0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[0]);

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state()->index == 2;
  }));
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(1);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());
}

// Tests that Find in Page does not respond to a FindInPageNext or a
// FindInPagePrevious call if no FindInPageSearch find was executed beforehand.
TEST_F(JavaScriptFindInPageManagerImplTest,
       FindDidNotRepondToNextOrPrevIfNoSearch) {
  auto three = std::make_unique<base::Value>(3.0);
  auto frame_with_three_matches =
      CreateMainWebFrameWithJsResultForFind(three.get());
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
TEST_F(JavaScriptFindInPageManagerImplTest,
       FindDidHighlightNextMatchAfterFrameDisappears) {
  auto one = std::make_unique<base::Value>(1.0);
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  FakeWebFrame* frame_with_one_match_ptr = frame_with_one_match.get();
  auto frame_with_two_matches =
      CreateChildWebFrameWithJsResultForFind(two.get());
  FakeWebFrame* frame_with_two_matches_ptr = frame_with_two_matches.get();
  AddWebFrame(std::move(frame_with_one_match));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));
  ASSERT_EQ(2ul, frame_with_one_match_ptr->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_one_match_ptr->GetJavaScriptCallHistory()[1]);

  RemoveWebFrame(kMainFakeFrameId);
  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state()->index == 0;
  }));
  EXPECT_EQ(u"__gCrWeb.findInPage.selectAndScrollToVisibleMatch(0);",
            frame_with_two_matches_ptr->GetLastJavaScriptCall());
}

// Tests that Find in Page does not respond when frame is removed
TEST_F(JavaScriptFindInPageManagerImplTest, FindDidNotRepondAfterFrameRemoved) {
  auto one = std::make_unique<base::Value>(1.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  AddWebFrame(std::move(frame_with_one_match));

  RemoveWebFrame(kMainFakeFrameId);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_delegate_.state());
}

// Tests that Find in Page responds with a total match count of one to a
// FindInPageSearch find when there is one match in a frame and then responds
// with a total match count of zero when that frame is removed.
TEST_F(JavaScriptFindInPageManagerImplTest,
       FindInPageUpdateMatchCountAfterFrameRemoved) {
  auto one = std::make_unique<base::Value>(1.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
  AddWebFrame(std::move(frame_with_one_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));

  RemoveWebFrame(kMainFakeFrameId);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state()->match_count == 0;
  }));
}

// Tests that DidHighlightMatches is not called when a frame with no matches is
// removed from the page.
TEST_F(JavaScriptFindInPageManagerImplTest,
       FindDidNotResponseAfterFrameDisappears) {
  auto zero = std::make_unique<base::Value>(0.0);
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_zero_matches =
      CreateMainWebFrameWithJsResultForFind(zero.get());
  auto frame_with_two_matches =
      CreateChildWebFrameWithJsResultForFind(two.get());
  AddWebFrame(std::move(frame_with_zero_matches));
  AddWebFrame(std::move(frame_with_two_matches));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state();
  }));

  fake_delegate_.Reset();
  RemoveWebFrame(kMainFakeFrameId);

  EXPECT_FALSE(fake_delegate_.state());
}

// Tests that Find in Page SetContentIsHTML() returns true if the web state's
// content is HTML and returns false if the web state's content is not HTML.
TEST_F(JavaScriptFindInPageManagerImplTest, FindInPageCanSearchContent) {
  fake_web_state_->SetContentIsHTML(false);

  EXPECT_FALSE(GetFindInPageManager()->CanSearchContent());

  fake_web_state_->SetContentIsHTML(true);

  EXPECT_TRUE(GetFindInPageManager()->CanSearchContent());
}

// Tests that Find in Page resets the match count to 0 and the query to nil
// after calling StopFinding().
TEST_F(JavaScriptFindInPageManagerImplTest, FindInPageCanStopFind) {
  auto one = std::make_unique<base::Value>(1.0);
  auto frame_with_one_match = CreateMainWebFrameWithJsResultForFind(one.get());
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
TEST_F(JavaScriptFindInPageManagerImplTest, FindInPageNextUpdatesMatchCount) {
  auto two = std::make_unique<base::Value>(2.0);
  auto frame_with_hidden_match =
      CreateMainWebFrameWithJsResultForFind(two.get());
  FakeWebFrame* frame_with_hidden_match_ptr = frame_with_hidden_match.get();
  AddWebFrame(std::move(frame_with_hidden_match));

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state() && fake_delegate_.state()->match_count == 2;
  }));
  base::Value::Dict select_and_scroll_result;
  select_and_scroll_result.Set("matches", 3.0);
  select_and_scroll_result.Set("index", 1.0);
  base::Value select_and_scroll_result_value(
      std::move(select_and_scroll_result));
  frame_with_hidden_match_ptr->AddJsResultForFunctionCall(
      &select_and_scroll_result_value, kFindInPageSelectAndScrollToMatch);

  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state() && fake_delegate_.state()->match_count == 3;
  }));
  EXPECT_EQ(1, fake_delegate_.state()->index);
}

// Tests that Find in Page logs correct UserActions for given API calls.
TEST_F(JavaScriptFindInPageManagerImplTest, FindUserActions) {
  // Setup fake page with three matches.
  auto three = std::make_unique<base::Value>(3.0);
  auto frame_with_three_matches =
      CreateMainWebFrameWithJsResultForFind(three.get());
  AddWebFrame(std::move(frame_with_three_matches));

  ASSERT_EQ(0,
            user_action_tester_.GetActionCount("IOS.FindInPage.SearchStarted"));
  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageSearch);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("IOS.FindInPage.SearchStarted"));

  // Wait for JavaScript completion. This is required as the FindNext
  // and FindPrevious user actions are only recorded if a sufficient number of
  // matches has been reported to the manager (>2).
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fake_delegate_.state() && fake_delegate_.state()->match_count == 3;
  }));

  ASSERT_EQ(0, user_action_tester_.GetActionCount("IOS.FindInPage.FindNext"));
  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPageNext);
  EXPECT_EQ(1, user_action_tester_.GetActionCount("IOS.FindInPage.FindNext"));

  ASSERT_EQ(0,
            user_action_tester_.GetActionCount("IOS.FindInPage.FindPrevious"));
  GetFindInPageManager()->Find(@"foo", FindInPageOptions::FindInPagePrevious);
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount("IOS.FindInPage.FindPrevious"));
}

}  // namespace web
