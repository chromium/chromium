// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_fetcher_impl.h"

#include "base/memory/ptr_util.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::StrictMock;

namespace content {

namespace {

class MockContentBrowserClient : public ContentBrowserClient {
 public:
  MockContentBrowserClient() = default;
  ~MockContentBrowserClient() override = default;

  MOCK_METHOD3(FetchRemoteSms,
               void(BrowserContext*,
                    const url::Origin&,
                    base::OnceCallback<void(base::Optional<std::string>)>));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockContentBrowserClient);
};

class MockSubscriber : public SmsFetcher::Subscriber {
 public:
  MockSubscriber() = default;
  ~MockSubscriber() override = default;

  MOCK_METHOD1(OnReceive, void(const std::string& one_time_code));
  MOCK_METHOD1(OnFailure, void(SmsFetcher::FailureType failure_type));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSubscriber);
};

class SmsFetcherImplTest : public RenderViewHostTestHarness {
 public:
  SmsFetcherImplTest() = default;
  ~SmsFetcherImplTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    original_client_ = SetBrowserClientForTesting(&client_);
  }

  void TearDown() override {
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  MockContentBrowserClient* client() { return &client_; }
  MockSmsProvider* provider() { return &provider_; }

 private:
  ContentBrowserClient* original_client_ = nullptr;
  NiceMock<MockContentBrowserClient> client_;
  NiceMock<MockSmsProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(SmsFetcherImplTest);
};

}  // namespace

TEST_F(SmsFetcherImplTest, ReceiveFromLocalSmsProvider) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://a.com"));

  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(nullptr, provider());

  EXPECT_CALL(*provider(), Retrieve(_)).WillOnce(Invoke([&]() {
    provider()->NotifyReceive(kOrigin, "123");
  }));

  EXPECT_CALL(subscriber, OnReceive("123"));

  fetcher.Subscribe(kOrigin, &subscriber, main_rfh());
}

TEST_F(SmsFetcherImplTest, ReceiveFromRemoteProvider) {
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(nullptr, provider());

  const std::string& sms = "@a.com #123";

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(Invoke(
          [&](BrowserContext*, const url::Origin&,
              base::OnceCallback<void(base::Optional<std::string>)> callback) {
            std::move(callback).Run(sms);
          }));

  EXPECT_CALL(subscriber, OnReceive("123"));

  fetcher.Subscribe(url::Origin::Create(GURL("https://a.com")), &subscriber,
                    main_rfh());
}

TEST_F(SmsFetcherImplTest, RemoteProviderTimesOut) {
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(nullptr, provider());

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(Invoke(
          [&](BrowserContext*, const url::Origin&,
              base::OnceCallback<void(base::Optional<std::string>)> callback) {
            std::move(callback).Run(base::nullopt);
          }));

  EXPECT_CALL(subscriber, OnReceive(_)).Times(0);

  fetcher.Subscribe(url::Origin::Create(GURL("https://a.com")), &subscriber,
                    main_rfh());
}

TEST_F(SmsFetcherImplTest, ReceiveFromOtherOrigin) {
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(nullptr, provider());

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(Invoke(
          [&](BrowserContext*, const url::Origin&,
              base::OnceCallback<void(base::Optional<std::string>)> callback) {
            std::move(callback).Run("@b.com #123");
          }));

  EXPECT_CALL(subscriber, OnReceive(_)).Times(0);

  fetcher.Subscribe(url::Origin::Create(GURL("https://a.com")), &subscriber,
                    main_rfh());
}

TEST_F(SmsFetcherImplTest, ReceiveFromBothProviders) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://a.com"));
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(nullptr, provider());

  const std::string& sms = "hello\n@a.com #123";

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(Invoke(
          [&](BrowserContext*, const url::Origin&,
              base::OnceCallback<void(base::Optional<std::string>)> callback) {
            std::move(callback).Run(sms);
          }));

  EXPECT_CALL(*provider(), Retrieve(_)).WillOnce(Invoke([&]() {
    provider()->NotifyReceive(kOrigin, sms);
  }));

  // Expects subscriber to be notified just once.
  EXPECT_CALL(subscriber, OnReceive("123"));

  fetcher.Subscribe(kOrigin, &subscriber, main_rfh());
}

TEST_F(SmsFetcherImplTest, OneOriginTwoSubscribers) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://a.com"));

  StrictMock<MockSubscriber> subscriber1;
  StrictMock<MockSubscriber> subscriber2;

  SmsFetcherImpl fetcher(nullptr, provider());

  fetcher.Subscribe(kOrigin, &subscriber1, main_rfh());
  fetcher.Subscribe(kOrigin, &subscriber2, main_rfh());

  EXPECT_CALL(subscriber1, OnReceive("123"));
  provider()->NotifyReceive(kOrigin, "123");

  EXPECT_CALL(subscriber2, OnReceive("456"));
  provider()->NotifyReceive(kOrigin, "456");
}

TEST_F(SmsFetcherImplTest, TwoOriginsTwoSubscribers) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://a.com"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://b.com"));

  StrictMock<MockSubscriber> subscriber1;
  StrictMock<MockSubscriber> subscriber2;

  SmsFetcherImpl fetcher(nullptr, provider());
  fetcher.Subscribe(kOrigin1, &subscriber1, main_rfh());
  fetcher.Subscribe(kOrigin2, &subscriber2, main_rfh());

  EXPECT_CALL(subscriber2, OnReceive("456"));
  provider()->NotifyReceive(kOrigin2, "456");

  EXPECT_CALL(subscriber1, OnReceive("123"));
  provider()->NotifyReceive(kOrigin1, "123");
}

}  // namespace content
