// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/public/cpp/test/fake_navigable_contents_factory.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/http/http_response_headers.h"
#include "services/content/public/cpp/navigable_contents.h"
#include "services/content/public/cpp/navigable_contents_observer.h"
#include "services/content/public/cpp/test/fake_navigable_contents.h"
#include "services/content/public/mojom/navigable_contents.mojom.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

class FakeNavigableContentsFactoryTest : public testing::Test {
 public:
  FakeNavigableContentsFactoryTest() {
    factory_.BindReceiver(remote_factory_.BindNewPipeAndPassReceiver());
  }

  ~FakeNavigableContentsFactoryTest() override = default;

  FakeNavigableContentsFactory& factory() { return factory_; }

  mojom::NavigableContentsFactory* remote_factory() {
    return remote_factory_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  mojo::Remote<mojom::NavigableContentsFactory> remote_factory_;
  FakeNavigableContentsFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeNavigableContentsFactoryTest);
};

class NavigationObserver : public NavigableContentsObserver {
 public:
  explicit NavigationObserver(NavigableContents* contents)
      : contents_(contents) {
    contents_->AddObserver(this);
  }

  ~NavigationObserver() override { contents_->RemoveObserver(this); }

  const GURL& last_url() const { return last_url_; }
  const scoped_refptr<net::HttpResponseHeaders>& last_response_headers() const {
    return last_response_headers_;
  }
  size_t navigations_observed() const { return navigations_observed_; }
  bool last_navigation_succeeded() const { return last_navigation_succeeded_; }

  void WaitForNavigation() {
    if (!navigation_run_loop_)
      navigation_run_loop_.emplace();
    navigation_run_loop_->Run();
    navigation_run_loop_.reset();
  }

 private:
  // NavigableContentsObserver:
  void DidFinishNavigation(
      const GURL& url,
      bool is_main_frame,
      bool is_error_page,
      const net::HttpResponseHeaders* response_headers) override {
    last_url_ = url;
    last_navigation_succeeded_ = !is_error_page;
    if (response_headers) {
      last_response_headers_ = base::MakeRefCounted<net::HttpResponseHeaders>(
          response_headers->raw_headers());
    } else {
      last_response_headers_ = nullptr;
    }
    ++navigations_observed_;
    if (navigation_run_loop_)
      navigation_run_loop_->Quit();
  }

  NavigableContents* const contents_;
  GURL last_url_;
  scoped_refptr<net::HttpResponseHeaders> last_response_headers_;
  bool last_navigation_succeeded_ = false;
  size_t navigations_observed_ = 0;
  base::Optional<base::RunLoop> navigation_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

TEST_F(FakeNavigableContentsFactoryTest, BasicNavigation) {
  NavigableContents contents(remote_factory());
  FakeNavigableContents contents_impl;
  factory().WaitForAndBindNextContentsRequest(&contents_impl);

  const GURL kTestUrl("https://www.google.com/");
  contents.Navigate(kTestUrl);

  NavigationObserver observer(&contents);
  observer.WaitForNavigation();

  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(1u, observer.navigations_observed());
  EXPECT_EQ(kTestUrl, observer.last_url());
  EXPECT_EQ(nullptr, observer.last_response_headers());
}

TEST_F(FakeNavigableContentsFactoryTest, MultipleClients) {
  NavigableContents contents1(remote_factory());
  FakeNavigableContents contents1_impl;
  factory().WaitForAndBindNextContentsRequest(&contents1_impl);

  NavigableContents contents2(remote_factory());
  FakeNavigableContents contents2_impl;
  factory().WaitForAndBindNextContentsRequest(&contents2_impl);

  const GURL kTestUrl1("https://www.google.com/?q=cats");
  const GURL kTestUrl2("https://www.google.com/?q=dogs");
  contents1.Navigate(kTestUrl1);
  contents2.Navigate(kTestUrl2);

  NavigationObserver observer1(&contents1);
  NavigationObserver observer2(&contents2);
  observer1.WaitForNavigation();
  observer2.WaitForNavigation();

  EXPECT_TRUE(observer1.last_navigation_succeeded());
  EXPECT_EQ(1u, observer1.navigations_observed());
  EXPECT_EQ(kTestUrl1, observer1.last_url());
  EXPECT_EQ(nullptr, observer1.last_response_headers());

  EXPECT_TRUE(observer2.last_navigation_succeeded());
  EXPECT_EQ(1u, observer2.navigations_observed());
  EXPECT_EQ(kTestUrl2, observer2.last_url());
  EXPECT_EQ(nullptr, observer2.last_response_headers());
}

TEST_F(FakeNavigableContentsFactoryTest, CustomHeaders) {
  NavigableContents contents(remote_factory());
  FakeNavigableContents contents_impl;
  factory().WaitForAndBindNextContentsRequest(&contents_impl);

  const std::string kTestHeader1 = "Test-Header-1";
  const std::string kTestHeaderValue1 = "apples";
  const std::string kTestHeader2 = "Test-Header-2";
  const std::string kTestHeaderValue2 = "bananas";
  auto test_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  test_headers->AddHeader(kTestHeader1 + ": " + kTestHeaderValue1);
  test_headers->AddHeader(kTestHeader2 + ": " + kTestHeaderValue2);
  contents_impl.set_default_response_headers(test_headers);

  const GURL kTestUrl("https://www.google.com/");
  contents.Navigate(kTestUrl);

  NavigationObserver observer(&contents);
  observer.WaitForNavigation();

  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(kTestUrl, observer.last_url());
  ASSERT_TRUE(observer.last_response_headers());
  EXPECT_TRUE(observer.last_response_headers()->HasHeaderValue(
      kTestHeader1, kTestHeaderValue1));
  EXPECT_TRUE(observer.last_response_headers()->HasHeaderValue(
      kTestHeader2, kTestHeaderValue2));
}

}  // namespace
}  // namespace content
