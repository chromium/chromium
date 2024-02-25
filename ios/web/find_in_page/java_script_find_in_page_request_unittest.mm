// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/java_script_find_in_page_request.h"

#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace web {

class JavaScriptFindInPageRequestTest : public WebTest {
 protected:
  JavaScriptFindInPageRequestTest() {
    auto main_frame = FakeWebFrame::CreateMainWebFrame(GURL());
    request_.AddFrame(main_frame.get());
    auto frame_with_two_matches = FakeWebFrame::CreateChildWebFrame(GURL());
    request_.AddFrame(frame_with_two_matches.get());
    request_.Reset(@"foo", 2);
    request_.SetMatchCountForFrame(1, kMainFakeFrameId);
    request_.SetMatchCountForFrame(2, kChildFakeFrameId);
  }
  JavaScriptFindInPageRequest request_;
};

// Tests that JavaScriptFindInPageRequest properly clears its properties in
// respond to a Reset() call.
TEST_F(JavaScriptFindInPageRequestTest, Reset) {
  EXPECT_EQ(3, request_.GetTotalMatchCount());

  EXPECT_TRUE(request_.GoToFirstMatch());

  EXPECT_EQ(0, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(1, request_.GetMatchCountForSelectedFrame());

  int request_id = request_.GetRequestId();
  request_.Reset(@"foobar", 2);

  EXPECT_GE(request_.GetRequestId(), request_id);
  EXPECT_EQ(-1, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(@"foobar", request_.GetRequestQuery());
  EXPECT_EQ(0, request_.GetTotalMatchCount());
  EXPECT_EQ(-1, request_.GetMatchCountForSelectedFrame());
}

// Tests that JavaScriptFindInPageRequest properly decrements
// `pending_frame_call_count_` properly.
TEST_F(JavaScriptFindInPageRequestTest, AllFindResponsesReturned) {
  request_.DidReceiveFindResponseFromOneFrame();
  EXPECT_FALSE(request_.AreAllFindResponsesReturned());

  request_.DidReceiveFindResponseFromOneFrame();
  EXPECT_TRUE(request_.AreAllFindResponsesReturned());
}

// Tests that JavaScriptFindInPageRequest GoToNextMatch() is able to traverse
// all matches in multiple frames.
TEST_F(JavaScriptFindInPageRequestTest, GoToNext) {
  request_.GoToFirstMatch();

  EXPECT_EQ(0, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(0, request_.GetCurrentSelectedMatchPageIndex());

  request_.GoToNextMatch();

  EXPECT_EQ(0, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(1, request_.GetCurrentSelectedMatchPageIndex());

  request_.GoToNextMatch();

  EXPECT_EQ(1, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(2, request_.GetCurrentSelectedMatchPageIndex());

  request_.GoToNextMatch();

  EXPECT_EQ(0, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(0, request_.GetCurrentSelectedMatchPageIndex());
}

// Tests that JavaScriptFindInPageRequest GoToPreviousMatch() is able to
// traverse all matches in multiple frames.
TEST_F(JavaScriptFindInPageRequestTest, GoToPrevious) {
  request_.GoToFirstMatch();

  EXPECT_EQ(0, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(0, request_.GetCurrentSelectedMatchPageIndex());

  request_.GoToPreviousMatch();

  EXPECT_EQ(1, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(2, request_.GetCurrentSelectedMatchPageIndex());

  request_.GoToPreviousMatch();

  EXPECT_EQ(0, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(1, request_.GetCurrentSelectedMatchPageIndex());

  request_.GoToPreviousMatch();

  EXPECT_EQ(0, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(0, request_.GetCurrentSelectedMatchPageIndex());
}

// Tests that JavaScriptFindInPageRequest returns the correct relative match
// count within a frame and total match count when traversing matches in
// multiple frames.
TEST_F(JavaScriptFindInPageRequestTest, RelativeMatchCount) {
  request_.GoToFirstMatch();

  EXPECT_EQ(3, request_.GetTotalMatchCount());
  EXPECT_EQ(1, request_.GetMatchCountForSelectedFrame());

  request_.GoToNextMatch();

  EXPECT_EQ(3, request_.GetTotalMatchCount());
  EXPECT_EQ(2, request_.GetMatchCountForSelectedFrame());
}

// Tests that JavaScriptFindInPageRequest returns the correct relative match
// count within a frame and total match count when a frame is removed. Also
// tests that going to the next match after removing the currently selected
// frame produces the expected relative and total selected match index.
TEST_F(JavaScriptFindInPageRequestTest, RemoveFrame) {
  request_.GoToFirstMatch();

  EXPECT_EQ(3, request_.GetTotalMatchCount());
  EXPECT_EQ(1, request_.GetMatchCountForSelectedFrame());

  request_.RemoveFrame(kMainFakeFrameId);

  EXPECT_EQ(2, request_.GetTotalMatchCount());

  request_.GoToNextMatch();

  EXPECT_EQ(0, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(0, request_.GetCurrentSelectedMatchPageIndex());
}

// Tests that JavaScriptFindInPageRequest returns the correct relative match
// count within a frame and total match count when the match count for the
// currently selected frame changes.
TEST_F(JavaScriptFindInPageRequestTest, SetMatchCountForSelectedFrame) {
  request_.GoToFirstMatch();
  request_.SetMatchCountForSelectedFrame(5);

  EXPECT_EQ(7, request_.GetTotalMatchCount());
  EXPECT_EQ(5, request_.GetMatchCountForSelectedFrame());
}

// Tests that JavaScriptFindInPageRequest returns the currently selected match
// index relative to the frame and the total are correct when the total matches
// and the relative match index change.
TEST_F(JavaScriptFindInPageRequestTest, SetCurrentSelectedMatchIndex) {
  request_.GoToFirstMatch();
  request_.SetMatchCountForSelectedFrame(5);
  request_.SetCurrentSelectedMatchFrameIndex(1);

  EXPECT_EQ(1, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(1, request_.GetCurrentSelectedMatchPageIndex());
}

// Tests that JavaScriptFindInPageRequest returns the correct match count within
// a frame and total match count when the match count for a not currently
// selected frame changes.
TEST_F(JavaScriptFindInPageRequestTest, SetMatchCountForFrame) {
  request_.GoToFirstMatch();

  EXPECT_EQ(3, request_.GetTotalMatchCount());
  EXPECT_EQ(1, request_.GetMatchCountForSelectedFrame());

  request_.SetMatchCountForFrame(5, kChildFakeFrameId);

  EXPECT_EQ(6, request_.GetTotalMatchCount());
  EXPECT_EQ(1, request_.GetMatchCountForSelectedFrame());
}

}  // namespace web
