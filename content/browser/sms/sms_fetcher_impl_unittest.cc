// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_fetcher_impl.h"

#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace content {

namespace {

const url::Origin kTestOrigin = url::Origin::Create(GURL("https://a.com"));

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

  MOCK_METHOD2(OnReceive,
               void(const std::string& one_time_code, const std::string& sms));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSubscriber);
};

class SmsFetcherImplTest : public testing::Test {
 public:
  SmsFetcherImplTest() = default;
  ~SmsFetcherImplTest() override = default;

  void SetUp() override {
    original_client_ = SetBrowserClientForTesting(&client_);
  }

  void TearDown() override {
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
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
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(nullptr, provider());

  EXPECT_CALL(*provider(), Retrieve()).WillOnce(Invoke([&]() {
    provider()->NotifyReceive(kTestOrigin, "123", "hello");
  }));

  EXPECT_CALL(subscriber, OnReceive("123", "hello"));

  fetcher.Subscribe(kTestOrigin, &subscriber);
}

TEST_F(SmsFetcherImplTest, ReceiveFromRemoteProvider) {
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(nullptr, provider());

  const std::string& sms = "For: https://a.com?otp=123";

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(Invoke(
          [&](BrowserContext*, const url::Origin&,
              base::OnceCallback<void(base::Optional<std::string>)> callback) {
            std::move(callback).Run(sms);
          }));

  EXPECT_CALL(subscriber, OnReceive("123", sms));

  fetcher.Subscribe(kTestOrigin, &subscriber);
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

  EXPECT_CALL(subscriber, OnReceive(_, _)).Times(0);

  fetcher.Subscribe(kTestOrigin, &subscriber);
}

TEST_F(SmsFetcherImplTest, ReceiveFromOtherOrigin) {
  StrictMock<MockSubscriber> subscriber;
  const url::Origin origin = url::Origin::Create(GURL("https://a.com"));

  SmsFetcherImpl fetcher(nullptr, provider());

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(Invoke(
          [&](BrowserContext*, const url::Origin&,
              base::OnceCallback<void(base::Optional<std::string>)> callback) {
            std::move(callback).Run("For: https://b.com?otp=123");
          }));

  EXPECT_CALL(subscriber, OnReceive(_, _)).Times(0);

  fetcher.Subscribe(origin, &subscriber);
}

TEST_F(SmsFetcherImplTest, ReceiveFromBothProviders) {
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(nullptr, provider());

  const std::string& sms = "hello \nFor: https://a.com?otp=123";

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(Invoke(
          [&](BrowserContext*, const url::Origin&,
              base::OnceCallback<void(base::Optional<std::string>)> callback) {
            std::move(callback).Run(sms);
          }));

  EXPECT_CALL(*provider(), Retrieve()).WillOnce(Invoke([&]() {
    provider()->NotifyReceive(sms);
  }));

  // Expects subscriber to be notified just once.
  EXPECT_CALL(subscriber, OnReceive("123", sms));

  fetcher.Subscribe(kTestOrigin, &subscriber);
}

TEST_F(SmsFetcherImplTest, OneOriginTwoSubscribers) {
  StrictMock<MockSubscriber> subscriber1;
  StrictMock<MockSubscriber> subscriber2;

  SmsFetcherImpl fetcher(nullptr, provider());

  fetcher.Subscribe(kTestOrigin, &subscriber1);
  fetcher.Subscribe(kTestOrigin, &subscriber2);

  EXPECT_CALL(subscriber1, OnReceive("123", "foo"));
  provider()->NotifyReceive(kTestOrigin, "123", "foo");

  EXPECT_CALL(subscriber2, OnReceive("456", "bar"));
  provider()->NotifyReceive(kTestOrigin, "456", "bar");
}

TEST_F(SmsFetcherImplTest, TwoOriginsTwoSubscribers) {
  StrictMock<MockSubscriber> subscriber1;
  StrictMock<MockSubscriber> subscriber2;

  const url::Origin origin1 = url::Origin::Create(GURL("https://a.com"));
  const url::Origin origin2 = url::Origin::Create(GURL("https://b.com"));

  SmsFetcherImpl fetcher(nullptr, provider());
  fetcher.Subscribe(origin1, &subscriber1);
  fetcher.Subscribe(origin2, &subscriber2);

  EXPECT_CALL(subscriber2, OnReceive("456", "bar"));
  provider()->NotifyReceive(origin2, "456", "bar");

  EXPECT_CALL(subscriber1, OnReceive("123", "foo"));
  provider()->NotifyReceive(origin1, "123", "foo");
}

}  // namespace content