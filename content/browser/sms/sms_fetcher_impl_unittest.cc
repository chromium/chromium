// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_fetcher_impl.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
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

using UserConsent = SmsFetcher::UserConsent;
using FailureType = SmsFetchFailureType;

namespace {

class MockContentBrowserClient : public ContentBrowserClient {
 public:
  MockContentBrowserClient() = default;

  MockContentBrowserClient(const MockContentBrowserClient&) = delete;
  MockContentBrowserClient& operator=(const MockContentBrowserClient&) = delete;

  ~MockContentBrowserClient() override = default;

  MOCK_METHOD3(
      FetchRemoteSms,
      base::OnceClosure(WebContents*,
                        const std::vector<url::Origin>&,
                        base::OnceCallback<void(std::optional<OriginList>,
                                                std::optional<std::string>,
                                                std::optional<FailureType>)>));
};

class MockSubscriber : public SmsFetcher::Subscriber {
 public:
  MockSubscriber() = default;

  MockSubscriber(const MockSubscriber&) = delete;
  MockSubscriber& operator=(const MockSubscriber&) = delete;

  ~MockSubscriber() override = default;

  MOCK_METHOD3(OnReceive,
               void(const OriginList&,
                    const std::string& one_time_code,
                    UserConsent));
  MOCK_METHOD1(OnFailure, void(FailureType failure_type));
};

class SmsFetcherImplTest : public RenderViewHostTestHarness {
 public:
  SmsFetcherImplTest() = default;

  SmsFetcherImplTest(const SmsFetcherImplTest&) = delete;
  SmsFetcherImplTest& operator=(const SmsFetcherImplTest&) = delete;

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
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
  NiceMock<MockContentBrowserClient> client_;
  NiceMock<MockSmsProvider> provider_;
};

}  // namespace

TEST_F(SmsFetcherImplTest, ReceiveFromLocalSmsProvider) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://a.com"));

  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(provider());

  EXPECT_CALL(*provider(), Retrieve(_, _)).WillOnce(Invoke([&]() {
    provider()->NotifyReceive(OriginList{kOrigin}, "123",
                              UserConsent::kObtained);
  }));

  EXPECT_CALL(subscriber, OnReceive(_, "123", UserConsent::kObtained));

  fetcher.Subscribe(OriginList{kOrigin}, subscriber, *main_rfh());
}

TEST_F(SmsFetcherImplTest, ReceiveFromRemoteProvider) {
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(provider());

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(
          Invoke([&](WebContents*, const OriginList&,
                     base::OnceCallback<void(
                         std::optional<OriginList>, std::optional<std::string>,
                         std::optional<FailureType>)> callback) {
            std::move(callback).Run(
                OriginList{url::Origin::Create(GURL("https://a.com"))}, "123",
                std::nullopt);
            return base::NullCallback();
          }));

  EXPECT_CALL(subscriber, OnReceive(_, "123", _));

  fetcher.Subscribe(OriginList{url::Origin::Create(GURL("https://a.com"))},
                    subscriber, *main_rfh());
}

TEST_F(SmsFetcherImplTest, RemoteProviderTimesOut) {
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(provider());

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(
          Invoke([&](WebContents*, const OriginList&,
                     base::OnceCallback<void(
                         std::optional<OriginList>, std::optional<std::string>,
                         std::optional<FailureType>)> callback) {
            std::move(callback).Run(std::nullopt, std::nullopt, std::nullopt);
            return base::NullCallback();
          }));

  EXPECT_CALL(subscriber, OnReceive(_, _, _)).Times(0);

  fetcher.Subscribe(OriginList{url::Origin::Create(GURL("https://a.com"))},
                    subscriber, *main_rfh());
}

TEST_F(SmsFetcherImplTest, ReceiveFromOtherOrigin) {
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(provider());

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(
          Invoke([&](WebContents*, const OriginList&,
                     base::OnceCallback<void(
                         std::optional<OriginList>, std::optional<std::string>,
                         std::optional<FailureType>)> callback) {
            std::move(callback).Run(
                OriginList{url::Origin::Create(GURL("b.com"))}, "123",
                std::nullopt);
            return base::NullCallback();
          }));

  EXPECT_CALL(subscriber, OnReceive(_, _, _)).Times(0);

  fetcher.Subscribe(OriginList{url::Origin::Create(GURL("https://a.com"))},
                    subscriber, *main_rfh());
}

TEST_F(SmsFetcherImplTest, ReceiveFromBothProviders) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://a.com"));
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(provider());

  const std::string& sms = "hello\n@a.com #123";

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(
          Invoke([&](WebContents*, const OriginList&,
                     base::OnceCallback<void(
                         std::optional<OriginList>, std::optional<std::string>,
                         std::optional<FailureType>)> callback) {
            std::move(callback).Run(
                OriginList{url::Origin::Create(GURL("https://a.com"))}, "123",
                std::nullopt);
            return base::NullCallback();
          }));

  EXPECT_CALL(*provider(), Retrieve(_, _)).WillOnce(Invoke([&]() {
    provider()->NotifyReceive(OriginList{kOrigin}, sms,
                              UserConsent::kNotObtained);
  }));

  // Expects subscriber to be notified just once.
  EXPECT_CALL(subscriber, OnReceive(_, "123", UserConsent::kObtained));

  fetcher.Subscribe(OriginList{kOrigin}, subscriber, *main_rfh());
}

TEST_F(SmsFetcherImplTest, OneOriginTwoSubscribers) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://a.com"));

  StrictMock<MockSubscriber> subscriber1;
  StrictMock<MockSubscriber> subscriber2;

  SmsFetcherImpl fetcher(provider());

  fetcher.Subscribe(OriginList{kOrigin}, subscriber1, *main_rfh());
  fetcher.Subscribe(OriginList{kOrigin}, subscriber2, *main_rfh());

  EXPECT_CALL(subscriber1, OnReceive(_, "123", UserConsent::kObtained));
  provider()->NotifyReceive(OriginList{kOrigin}, "123", UserConsent::kObtained);

  EXPECT_CALL(subscriber2, OnReceive(_, "456", UserConsent::kObtained));
  provider()->NotifyReceive(OriginList{kOrigin}, "456", UserConsent::kObtained);
}

TEST_F(SmsFetcherImplTest, TwoOriginsTwoSubscribers) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://a.com"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://b.com"));

  StrictMock<MockSubscriber> subscriber1;
  StrictMock<MockSubscriber> subscriber2;

  SmsFetcherImpl fetcher(provider());
  fetcher.Subscribe(OriginList{kOrigin1}, subscriber1, *main_rfh());
  fetcher.Subscribe(OriginList{kOrigin2}, subscriber2, *main_rfh());

  EXPECT_CALL(subscriber2, OnReceive(_, "456", UserConsent::kObtained));
  provider()->NotifyReceive(OriginList{kOrigin2}, "456",
                            UserConsent::kObtained);

  EXPECT_CALL(subscriber1, OnReceive(_, "123", UserConsent::kObtained));
  provider()->NotifyReceive(OriginList{kOrigin1}, "123",
                            UserConsent::kObtained);
}

TEST_F(SmsFetcherImplTest, OneOriginTwoSubscribersOnlyOneIsNotifiedFailed) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://a.com"));

  StrictMock<MockSubscriber> subscriber1;
  StrictMock<MockSubscriber> subscriber2;

  SmsFetcherImpl fetcher1(provider());
  SmsFetcherImpl fetcher2(provider());

  fetcher1.Subscribe(OriginList{kOrigin}, subscriber1, *main_rfh());
  fetcher2.Subscribe(OriginList{kOrigin}, subscriber2, *main_rfh());

  EXPECT_CALL(subscriber1, OnFailure(FailureType::kPromptTimeout));
  EXPECT_CALL(subscriber2, OnFailure(FailureType::kPromptTimeout)).Times(0);
  provider()->NotifyFailure(FailureType::kPromptTimeout);
}

TEST_F(SmsFetcherImplTest, FetchRemoteSmsFailed) {
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(provider());

  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(
          Invoke([&](WebContents*, const OriginList&,
                     base::OnceCallback<void(
                         std::optional<OriginList>, std::optional<std::string>,
                         std::optional<FailureType>)> callback) {
            std::move(callback).Run(
                std::nullopt, std::nullopt,
                static_cast<FailureType>(FailureType::kPromptCancelled));
            return base::NullCallback();
          }));

  EXPECT_CALL(subscriber, OnFailure(_));

  fetcher.Subscribe(OriginList{url::Origin::Create(GURL("https://a.com"))},
                    subscriber, *main_rfh());
}

TEST_F(SmsFetcherImplTest, FetchRemoteSmsCancelled) {
  StrictMock<MockSubscriber> subscriber;
  SmsFetcherImpl fetcher(provider());

  base::MockOnceClosure cancel_callback;
  EXPECT_CALL(*client(), FetchRemoteSms(_, _, _))
      .WillOnce(
          Invoke([&](WebContents*, const OriginList&,
                     base::OnceCallback<void(
                         std::optional<OriginList>, std::optional<std::string>,
                         std::optional<FailureType>)> callback) {
            return cancel_callback.Get();
          }));

  EXPECT_CALL(cancel_callback, Run).Times(0);
  OriginList origin_list =
      OriginList{url::Origin::Create(GURL("https://a.com"))};
  fetcher.Subscribe(origin_list, subscriber, *main_rfh());

  testing::Mock::VerifyAndClearExpectations(&cancel_callback);
  EXPECT_CALL(cancel_callback, Run);
  fetcher.Unsubscribe(origin_list, &subscriber);
}

}  // namespace content
