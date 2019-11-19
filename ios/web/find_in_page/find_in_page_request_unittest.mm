// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_request.h"

#include "ios/web/public/test/fakes/fake_web_frame.h"
#include "ios/web/public/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kOneMatchFrameId[] = "frame_with_one_match";
const char kTwoMatchesFrameId[] = "frame_with_two_matches";
}  // namespace

namespace web {

class FindInPageRequestTest : public WebTest {
 protected:
  // Returns a FakeWebFrame with id |frame_id|.
  std::unique_ptr<FakeWebFrame> CreateWebFrame(const std::string& frame_id,
                                               bool is_main_frame) {
    return std::make_unique<FakeWebFrame>(frame_id, is_main_frame,
                                          GURL::EmptyGURL());
  }

  FindInPageRequestTest() {
    auto main_frame = CreateWebFrame(kOneMatchFrameId,
                                     /*is_main_frame=*/true);
    request_.AddFrame(main_frame.get());
    auto frame_with_two_matches =
        CreateWebFrame(kTwoMatchesFrameId, /*is_main_frame=*/false);
    request_.AddFrame(frame_with_two_matches.get());
    request_.Reset(@"foo", 2);
    request_.SetMatchCountForFrame(1, kOneMatchFrameId);
    request_.SetMatchCountForFrame(2, kTwoMatchesFrameId);
  }
  FindInPageRequest request_;
};

// Tests that FindInPageRequest properly clears its properties in respond to a
// Reset() call.
TEST_F(FindInPageRequestTest, Reset) {
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

// Tests that FindinPageRequest properly decrements |pending_frame_call_count_|
// properly.
TEST_F(FindInPageRequestTest, AllFindResponsesReturned) {
  request_.DidReceiveFindResponseFromOneFrame();
  EXPECT_FALSE(request_.AreAllFindResponsesReturned());

  request_.DidReceiveFindResponseFromOneFrame();
  EXPECT_TRUE(request_.AreAllFindResponsesReturned());
}

// Tests that FindInPageRequest GoToNextMatch() is able to traverse all matches
// in multiple frames.
TEST_F(FindInPageRequestTest, GoToNext) {
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

// Tests that FindInPageRequest GoToPreviousMatch() is able to traverse all
// matches in multiple frames.
TEST_F(FindInPageRequestTest, GoToPrevious) {
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

// Tests that FindInPageRequest returns the correct relative match count within
// a frame and total match count when traversing matches in multiple frames.
TEST_F(FindInPageRequestTest, RelativeMatchCount) {
  request_.GoToFirstMatch();

  EXPECT_EQ(3, request_.GetTotalMatchCount());
  EXPECT_EQ(1, request_.GetMatchCountForSelectedFrame());

  request_.GoToNextMatch();

  EXPECT_EQ(3, request_.GetTotalMatchCount());
  EXPECT_EQ(2, request_.GetMatchCountForSelectedFrame());
}

// Tests that FindInPageRequest returns the correct relative match count within
// a frame and total match count when a frame is removed. Also tests that going
// to the next match after removing the currently selected frame produces the
// expected relative and total selected match index.
TEST_F(FindInPageRequestTest, RemoveFrame) {
  request_.GoToFirstMatch();

  EXPECT_EQ(3, request_.GetTotalMatchCount());
  EXPECT_EQ(1, request_.GetMatchCountForSelectedFrame());

  request_.RemoveFrame(kOneMatchFrameId);

  EXPECT_EQ(2, request_.GetTotalMatchCount());

  request_.GoToNextMatch();

  EXPECT_EQ(0, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(0, request_.GetCurrentSelectedMatchPageIndex());
}

// Tests that FindInPageRequest returns the correct relative match count within
// a frame and total match count when the match count for the currently selected
// frame changes.
TEST_F(FindInPageRequestTest, SetMatchCountForSelectedFrame) {
  request_.GoToFirstMatch();
  request_.SetMatchCountForSelectedFrame(5);

  EXPECT_EQ(7, request_.GetTotalMatchCount());
  EXPECT_EQ(5, request_.GetMatchCountForSelectedFrame());
}

// Tests that FindInPageRequest returns the currently selected match index
// relative to the frame and the total are correct when the total matches and
// the relative match index change.
TEST_F(FindInPageRequestTest, SetCurrentSelectedMatchIndex) {
  request_.GoToFirstMatch();
  request_.SetMatchCountForSelectedFrame(5);
  request_.SetCurrentSelectedMatchFrameIndex(1);

  EXPECT_EQ(1, request_.GetCurrentSelectedMatchFrameIndex());
  EXPECT_EQ(1, request_.GetCurrentSelectedMatchPageIndex());
}

// Tests that FindInPageRequest returns the correct match count within
// a frame and total match count when the match count for a not currently
// selected frame changes.
TEST_F(FindInPageRequestTest, SetMatchCountForFrame) {
  request_.GoToFirstMatch();

  EXPECT_EQ(3, request_.GetTotalMatchCount());
  EXPECT_EQ(1, request_.GetMatchCountForSelectedFrame());

  request_.SetMatchCountForFrame(5, kTwoMatchesFrameId);

  EXPECT_EQ(6, request_.GetTotalMatchCount());
  EXPECT_EQ(1, request_.GetMatchCountForSelectedFrame());
}

}  // namespace web
