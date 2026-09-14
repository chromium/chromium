// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_encryption_protocol_provider.h"

#import <string>
#import <string_view>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace enterprise_reporting {

namespace {

class MockProber : public SaasUsageEncryptionProtocolProvider::Prober {
 public:
  MockProber() = default;
  ~MockProber() override = default;

  MOCK_METHOD(void,
              ProbeUrl,
              (const GURL& url,
               SaasUsageEncryptionProtocolProvider::EncryptionProtocolCallback
                   callback),
              (override));
};

}  // namespace

class SaasUsageEncryptionProtocolProviderTest : public PlatformTest {
 protected:
  SaasUsageEncryptionProtocolProviderTest() {
    auto prober = std::make_unique<MockProber>();
    prober_ = prober.get();
    provider_ = &SaasUsageEncryptionProtocolProvider::GetInstance();
    provider_->SetProberForTesting(std::move(prober));
  }

  ~SaasUsageEncryptionProtocolProviderTest() override {
    prober_ = nullptr;
    provider_->SetProberForTesting(nullptr);
  }

  base::test::TaskEnvironment task_environment_;
  raw_ptr<SaasUsageEncryptionProtocolProvider> provider_;
  raw_ptr<MockProber> prober_;
};

TEST_F(SaasUsageEncryptionProtocolProviderTest,
       NonCryptographicUrlReturnsUnencrypted) {
  EXPECT_CALL(*prober_, ProbeUrl).Times(0);
  base::test::TestFuture<std::string_view> future;
  provider_->GetEncryptionProtocol(GURL("http://example.com/login"),
                                   future.GetCallback());
  EXPECT_EQ(future.Get(), "Unencrypted");
}

TEST_F(SaasUsageEncryptionProtocolProviderTest,
       HttpsUrlProbesAndResolvesProtocol) {
  EXPECT_CALL(*prober_, ProbeUrl(GURL("https://example.com/app"), testing::_))
      .WillOnce(base::test::RunOnceCallback<1>("TLS 1.3"));
  base::test::TestFuture<std::string_view> future;
  provider_->GetEncryptionProtocol(GURL("https://example.com/app"),
                                   future.GetCallback());
  EXPECT_EQ(future.Get(), "TLS 1.3");
}

TEST_F(SaasUsageEncryptionProtocolProviderTest,
       DomainCachedSubsequentRequestsHitCache) {
  EXPECT_CALL(*prober_, ProbeUrl(GURL("https://example.com/page1"), testing::_))
      .WillOnce(base::test::RunOnceCallback<1>("TLS 1.2"));
  base::test::TestFuture<std::string_view> future1;
  provider_->GetEncryptionProtocol(GURL("https://example.com/page1"),
                                   future1.GetCallback());
  EXPECT_EQ(future1.Get(), "TLS 1.2");

  base::test::TestFuture<std::string_view> future2;
  provider_->GetEncryptionProtocol(GURL("https://example.com/page2"),
                                   future2.GetCallback());
  EXPECT_EQ(future2.Get(), "TLS 1.2");
}

TEST_F(SaasUsageEncryptionProtocolProviderTest,
       ConcurrentRequestsCoalesceToSingleProbe) {
  SaasUsageEncryptionProtocolProvider::EncryptionProtocolCallback
      probe_callback;
  EXPECT_CALL(*prober_, ProbeUrl(GURL("https://example.com/a"), testing::_))
      .WillOnce(
          [&](const GURL&,
              SaasUsageEncryptionProtocolProvider::EncryptionProtocolCallback
                  callback) { probe_callback = std::move(callback); });

  base::test::TestFuture<std::string_view> future1;
  base::test::TestFuture<std::string_view> future2;

  provider_->GetEncryptionProtocol(GURL("https://example.com/a"),
                                   future1.GetCallback());
  provider_->GetEncryptionProtocol(GURL("https://example.com/b"),
                                   future2.GetCallback());

  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  std::move(probe_callback).Run("TLS 1.3");

  EXPECT_EQ(future1.Get(), "TLS 1.3");
  EXPECT_EQ(future2.Get(), "TLS 1.3");
}

}  // namespace enterprise_reporting
