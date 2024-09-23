// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/navigation_controller_impl.h"

#include <string_view>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kUrl1[] = "http://www.url1.com/";
const char kUrl2[] = "http://www.url2.com/";
const char kTitle1[] = "title1";
const char kTitle2[] = "title2";

fuchsia::web::NavigationState CreateNavigationState(
    const GURL& url,
    std::string_view title,
    fuchsia::web::PageType page_type,
    bool can_go_back,
    bool can_go_forward,
    bool is_main_document_loaded) {
  fuchsia::web::NavigationState navigation_state;

  navigation_state.set_url(url.spec());
  navigation_state.set_title(std::string(title));
  navigation_state.set_page_type(fuchsia::web::PageType(page_type));
  navigation_state.set_can_go_back(can_go_back);
  navigation_state.set_can_go_forward(can_go_forward);
  navigation_state.set_is_main_document_loaded(is_main_document_loaded);

  return navigation_state;
}

}  // namespace

// Verifies that two NavigationStates that are the same are differenced
// correctly.
TEST(DiffNavigationEntriesTest, NoChange) {
  fuchsia::web::NavigationState state1 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, true, true);
  fuchsia::web::NavigationState state2 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, true, true);

  fuchsia::web::NavigationState difference;
  DiffNavigationEntriesForTest(state1, state2, &difference);
  EXPECT_TRUE(difference.IsEmpty());
}

// Differencing from an empty to non-empty state should return a diff equivalent
// to the non-empty state. Differencing to an empty state is not supported and
// should DCHECK.
TEST(DiffNavigationEntriesTest, EmptyAndNonEmpty) {
  fuchsia::web::NavigationState difference;
  fuchsia::web::NavigationState empty_state;
  fuchsia::web::NavigationState state = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, true, true);

  DiffNavigationEntriesForTest(empty_state, state, &difference);

  ASSERT_FALSE(difference.IsEmpty());
  ASSERT_TRUE(difference.has_title());
  EXPECT_EQ(difference.title(), kTitle1);
  ASSERT_TRUE(difference.has_url());
  EXPECT_EQ(difference.url(), kUrl1);

  difference = {};
  EXPECT_DCHECK_DEATH(
      DiffNavigationEntriesForTest(state, empty_state, &difference));
}

// Verifies that states with different URL and title are correctly checked.
TEST(DiffNavigationEntriesTest, DifferentTitleAndUrl) {
  fuchsia::web::NavigationState difference;
  fuchsia::web::NavigationState state1 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, true, true);
  fuchsia::web::NavigationState state2 = CreateNavigationState(
      GURL(kUrl2), kTitle2, fuchsia::web::PageType::NORMAL, true, true, true);

  DiffNavigationEntriesForTest(state1, state2, &difference);

  ASSERT_TRUE(difference.has_title());
  EXPECT_EQ(difference.title(), kTitle2);
  ASSERT_TRUE(difference.has_url());
  EXPECT_EQ(difference.url(), kUrl2);

  difference = {};
  DiffNavigationEntriesForTest(state2, state1, &difference);

  ASSERT_TRUE(difference.has_title());
  EXPECT_EQ(difference.title(), kTitle1);
  ASSERT_TRUE(difference.has_url());
  EXPECT_EQ(difference.url(), kUrl1);
}

// Verifies that differences are accumulated into |difference|.
TEST(DiffNavigationEntriesTest, DifferencesAccumulate) {
  fuchsia::web::NavigationState difference;
  fuchsia::web::NavigationState state1 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, true, true);
  fuchsia::web::NavigationState state2 = CreateNavigationState(
      GURL(kUrl2), kTitle1, fuchsia::web::PageType::NORMAL, true, true, true);
  fuchsia::web::NavigationState state3 = CreateNavigationState(
      GURL(kUrl2), kTitle2, fuchsia::web::PageType::NORMAL, true, true, true);

  DiffNavigationEntriesForTest(state1, state2, &difference);

  EXPECT_FALSE(difference.has_title());
  ASSERT_TRUE(difference.has_url());
  EXPECT_EQ(difference.url(), kUrl2);

  DiffNavigationEntriesForTest(state2, state3, &difference);

  ASSERT_TRUE(difference.has_title());
  EXPECT_EQ(difference.title(), kTitle2);
  ASSERT_TRUE(difference.has_url());
  EXPECT_EQ(difference.url(), kUrl2);
}

// Verifies that states with different can_go_back and can_go_forward are
// correctly checked.
TEST(DiffNavigationEntriesTest, DifferentCanGoBackAndForward) {
  fuchsia::web::NavigationState difference;
  fuchsia::web::NavigationState state1 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, false, true);
  fuchsia::web::NavigationState state2 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, false, true, true);

  DiffNavigationEntriesForTest(state1, state2, &difference);

  ASSERT_TRUE(difference.has_can_go_back());
  EXPECT_FALSE(difference.can_go_back());
  ASSERT_TRUE(difference.has_can_go_forward());
  EXPECT_TRUE(difference.can_go_forward());

  difference = {};
  DiffNavigationEntriesForTest(state2, state1, &difference);

  ASSERT_TRUE(difference.has_can_go_back());
  EXPECT_TRUE(difference.can_go_back());
  ASSERT_TRUE(difference.has_can_go_forward());
  EXPECT_FALSE(difference.can_go_forward());
}

// Verifies that is_main_document is checked correctly.
TEST(DiffNavigationEntriesTest, DifferentIsMainDocumentLoaded) {
  fuchsia::web::NavigationState difference;
  fuchsia::web::NavigationState state1 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, true, true);
  fuchsia::web::NavigationState state2 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, true, false);

  DiffNavigationEntriesForTest(state1, state2, &difference);
  ASSERT_TRUE(difference.has_is_main_document_loaded());
  EXPECT_FALSE(difference.is_main_document_loaded());

  difference = {};
  DiffNavigationEntriesForTest(state2, state1, &difference);
  ASSERT_TRUE(difference.has_is_main_document_loaded());
  EXPECT_TRUE(difference.is_main_document_loaded());
}

// Verifies that transitions from empty to non-empty states are handled.
TEST(FrameImplUnitTest, DiffNavigationEntriesFromInitial) {
  fuchsia::web::NavigationState difference;
  fuchsia::web::NavigationState state1;
  fuchsia::web::NavigationState state2 = CreateNavigationState(
      GURL(kUrl1), kTitle1, fuchsia::web::PageType::NORMAL, true, true, false);

  DiffNavigationEntriesForTest(state1, state2, &difference);
  EXPECT_FALSE(difference.IsEmpty());

  // Transitions from non-empty to empty (initial) state are DCHECK'd.
  EXPECT_DCHECK_DEATH(
      { DiffNavigationEntriesForTest(state2, state1, &difference); });
}

// Verifies that differencing between two empty/initial states are handled.
TEST(FrameImplUnitTest, DiffNavigationEntriesBothInitial) {
  fuchsia::web::NavigationState difference;
  fuchsia::web::NavigationState state1;
  fuchsia::web::NavigationState state2;

  DiffNavigationEntriesForTest(state1, state2, &difference);
  EXPECT_TRUE(difference.IsEmpty());
}
