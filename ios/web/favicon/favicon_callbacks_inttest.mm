// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

namespace {

// Observes and waits for FaviconUrlUpdated call.
class FaviconUrlObserver : public WebStateObserver {
 public:
  FaviconUrlObserver() = default;

  FaviconUrlObserver(const FaviconUrlObserver&) = delete;
  FaviconUrlObserver& operator=(const FaviconUrlObserver&) = delete;

  // Returns vavicon url candidates received in FaviconUrlUpdated.
  const std::vector<FaviconURL>& favicon_url_candidates() const {
    return favicon_url_candidates_;
  }
  // Returns true if FaviconUrlUpdated was called.
  bool favicon_url_updated() const { return favicon_url_updated_; }
  // WebStateObserver overrides:
  void FaviconUrlUpdated(WebState* web_state,
                         const std::vector<FaviconURL>& candidates) override {
    favicon_url_candidates_ = candidates;
    favicon_url_updated_ = true;
  }
  void WebStateDestroyed(WebState* web_state) override {
    NOTREACHED_IN_MIGRATION();
  }

 private:
  bool favicon_url_updated_ = false;
  std::vector<FaviconURL> favicon_url_candidates_;
};

}  // namespace

// Test fixture for WebStateDelegate::FaviconUrlUpdated and integration tests.
class FaviconCallbackTest : public web::WebTestWithWebState {
 public:
  FaviconCallbackTest() = default;

  FaviconCallbackTest(const FaviconCallbackTest&) = delete;
  FaviconCallbackTest& operator=(const FaviconCallbackTest&) = delete;

 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    web_state()->AddObserver(observer());
  }
  void TearDown() override {
    web_state()->RemoveObserver(observer());
    web::WebTestWithWebState::TearDown();
  }

  FaviconUrlObserver* observer() { return &observer_; }

 private:
  FaviconUrlObserver observer_;
};

// Tests page with shortcut icon link.
TEST_F(FaviconCallbackTest, ShortcutIconFavicon) {
  ASSERT_TRUE(observer()->favicon_url_candidates().empty());
  LoadHtml(@"<link rel='shortcut icon' href='http://fav.ico'>");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->favicon_url_updated();
  }));

  const std::vector<FaviconURL>& favicons =
      observer()->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
}

// Tests page with icon link and no sizes attribute.
TEST_F(FaviconCallbackTest, IconFavicon) {
  ASSERT_TRUE(observer()->favicon_url_candidates().empty());
  LoadHtml(@"<link rel='icon' href='http://fav.ico'>");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->favicon_url_updated();
  }));

  const std::vector<FaviconURL>& favicons =
      observer()->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
}

// Tests page with apple-touch-icon link.
TEST_F(FaviconCallbackTest, AppleTouchIconFavicon) {
  ASSERT_TRUE(observer()->favicon_url_candidates().empty());
  LoadHtml(@"<link rel='apple-touch-icon' href='http://fav.ico'>",
           GURL("https://chromium.test"));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->favicon_url_updated();
  }));

  const std::vector<FaviconURL>& favicons =
      observer()->favicon_url_candidates();
  ASSERT_EQ(2U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kTouchIcon, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
  EXPECT_EQ(GURL("https://chromium.test/favicon.ico"), favicons[1].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[1].icon_type);
  ASSERT_TRUE(favicons[1].icon_sizes.empty());
}

// Tests page with apple-touch-icon-precomposed link.
TEST_F(FaviconCallbackTest, AppleTouchIconPrecomposedFavicon) {
  ASSERT_TRUE(observer()->favicon_url_candidates().empty());
  LoadHtml(@"<link rel='apple-touch-icon-precomposed' href='http://fav.ico'>",
           GURL("https://chromium.test"));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->favicon_url_updated();
  }));

  const std::vector<FaviconURL>& favicons =
      observer()->favicon_url_candidates();
  ASSERT_EQ(2U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kTouchPrecomposedIcon, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
  EXPECT_EQ(GURL("https://chromium.test/favicon.ico"), favicons[1].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[1].icon_type);
  ASSERT_TRUE(favicons[1].icon_sizes.empty());
}

// Tests page without favicon link.
TEST_F(FaviconCallbackTest, NoFavicon) {
  ASSERT_TRUE(observer()->favicon_url_candidates().empty());
  LoadHtml(@"<html></html>", GURL("https://chromium.test/test/test.html"));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->favicon_url_updated();
  }));

  const std::vector<FaviconURL>& favicons =
      observer()->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  EXPECT_EQ(GURL("https://chromium.test/favicon.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
}

// Tests page without favicon link but with a query and a ref in the URL.
TEST_F(FaviconCallbackTest, NoFaviconWithQuery) {
  ASSERT_TRUE(observer()->favicon_url_candidates().empty());
  LoadHtml(@"<html></html>",
           GURL("https://chromium.test/test/test.html?q1#h1"));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->favicon_url_updated();
  }));

  const std::vector<FaviconURL>& favicons =
      observer()->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  EXPECT_EQ(GURL("https://chromium.test/favicon.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
}

// Tests page with multiple favicon links.
TEST_F(FaviconCallbackTest, MultipleFavicons) {
  ASSERT_TRUE(observer()->favicon_url_candidates().empty());
  LoadHtml(@"<link rel='shortcut icon' href='http://fav.ico'>"
            "<link rel='icon' href='http://fav1.ico'>"
            "<link rel='apple-touch-icon' href='http://fav2.ico'>"
            "<link rel='apple-touch-icon-precomposed' href='http://fav3.ico'>");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->favicon_url_updated();
  }));

  const std::vector<FaviconURL>& favicons =
      observer()->favicon_url_candidates();
  ASSERT_EQ(4U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
  EXPECT_EQ(GURL("http://fav1.ico"), favicons[1].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[1].icon_type);
  ASSERT_TRUE(favicons[1].icon_sizes.empty());
  EXPECT_EQ(GURL("http://fav2.ico"), favicons[2].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kTouchIcon, favicons[2].icon_type);
  ASSERT_TRUE(favicons[2].icon_sizes.empty());
  EXPECT_EQ(GURL("http://fav3.ico"), favicons[3].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kTouchPrecomposedIcon, favicons[3].icon_type);
  ASSERT_TRUE(favicons[3].icon_sizes.empty());
}

// Tests page with invalid favicon url.
TEST_F(FaviconCallbackTest, InvalidFaviconUrl) {
  ASSERT_TRUE(observer()->favicon_url_candidates().empty());
  LoadHtml(@"<html><head><link rel='icon' href='http://'></head></html>",
           GURL("https://chromium.test"));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->favicon_url_updated();
  }));

  const std::vector<FaviconURL>& favicons =
      observer()->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  EXPECT_EQ(GURL("https://chromium.test/favicon.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
}

// Tests page with empty favicon url.
TEST_F(FaviconCallbackTest, EmptyFaviconUrl) {
  ASSERT_TRUE(observer()->favicon_url_candidates().empty());
  LoadHtml(@"<head><link rel='icon' href=''></head>");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->favicon_url_updated();
  }));

  const std::vector<FaviconURL>& favicons =
      observer()->favicon_url_candidates();
  ASSERT_EQ(1U, favicons.size());
  // TODO(crbug.com/41319193): This result is not correct.
  EXPECT_EQ(GURL("https://chromium.test/"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[0].icon_type);
  ASSERT_TRUE(favicons[0].icon_sizes.empty());
}

// Tests page with icon links and a sizes attribute.
TEST_F(FaviconCallbackTest, IconFaviconSizes) {
  ASSERT_TRUE(observer()->favicon_url_candidates().empty());
  LoadHtml(
      @"<link rel='icon' href='http://fav.ico' sizes='10x20 30x40'><link "
      @"rel='apple-touch-icon' href='http://fav2.ico' sizes='10x20 asdfx'>");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->favicon_url_updated();
  }));

  const std::vector<FaviconURL>& favicons =
      observer()->favicon_url_candidates();
  ASSERT_EQ(2U, favicons.size());
  EXPECT_EQ(GURL("http://fav.ico"), favicons[0].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kFavicon, favicons[0].icon_type);

  ASSERT_EQ(2U, favicons[0].icon_sizes.size());
  EXPECT_EQ(10, favicons[0].icon_sizes[0].width());
  EXPECT_EQ(20, favicons[0].icon_sizes[0].height());
  EXPECT_EQ(30, favicons[0].icon_sizes[1].width());
  EXPECT_EQ(40, favicons[0].icon_sizes[1].height());

  EXPECT_EQ(GURL("http://fav2.ico"), favicons[1].icon_url);
  EXPECT_EQ(FaviconURL::IconType::kTouchIcon, favicons[1].icon_type);

  ASSERT_EQ(1U, favicons[1].icon_sizes.size());
  EXPECT_EQ(10, favicons[1].icon_sizes[0].width());
  EXPECT_EQ(20, favicons[1].icon_sizes[0].height());
}

}  // namespace web
