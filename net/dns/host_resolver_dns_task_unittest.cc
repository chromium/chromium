// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_dns_task.h"

#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/log/net_log_with_source.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_ANDROID)
#include <android/multinetwork.h>

#include "net/dns/dns_platform_attempt_factory_android.h"
#include "net/dns/mock_dns_platform_android_attempt_delegate.h"
#endif

namespace net {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class MockAddressSorter : public AddressSorter {
 public:
  MockAddressSorter() = default;
  ~MockAddressSorter() override = default;

  MOCK_METHOD(void,
              Sort,
              (const std::vector<IPEndPoint>& endpoints, CallbackType callback),
              (const, override));
};

class MockHostResolverDnsTaskDelegate : public HostResolverDnsTask::Delegate {
 public:
  MockHostResolverDnsTaskDelegate() = default;
  ~MockHostResolverDnsTaskDelegate() override = default;

  MOCK_METHOD(void,
              OnDnsTaskComplete,
              (base::TimeTicks start_time,
               bool allow_fallback,
               HostResolverDnsTask::Results results,
               DnsTransactionFactory::AttemptMode attempt_mode),
              (override));
  MOCK_METHOD(void,
              OnIntermediateTransactionsComplete,
              (std::optional<HostResolverDnsTask::SingleTransactionResults>
                   single_transaction_results),
              (override));
  MOCK_METHOD(RequestPriority, priority, (), (const, override));
  MOCK_METHOD(bool, IsHappyEyeballsV3Enabled, (), (const, override));
  MOCK_METHOD(void,
              AddTransactionTimeQueued,
              (base::TimeDelta time_queued),
              (override));
};

class HostResolverDnsTaskTest : public WithTaskEnvironment,
                                public testing::Test {
 public:
  HostResolverDnsTaskTest() = default;

 protected:
  void SetUp() override {
    auto context_builder = CreateTestURLRequestContextBuilder();
#if BUILDFLAG(IS_ANDROID)
    context_builder->set_dns_platform_attempt_factory(
        DnsPlatformAttemptFactoryAndroid::CreateForTesting(
            &mock_dns_platform_android_attempt_delegate_));
#endif
    request_context_ = context_builder->Build();
    resolve_context_ = std::make_unique<ResolveContext>(
        request_context_.get(), /*enable_caching=*/false);
    dns_client_ = DnsClient::CreateClient(/*net_log=*/nullptr);
    DnsConfig config;
    // The IP address doesn't matter; it's only necessary for SetSystemConfig to
    // have an effect.
    config.nameservers.emplace_back(IPAddress(192, 168, 1, 5), 53);
    CHECK(dns_client_->SetSystemConfig(config));
    // Allow non-DnsTransactionFactory::AttemptMode::kHttp attempts to be made.
    dns_client_->SetInsecureEnabled(/*enabled=*/true,
                                    /*additional_types_enabled=*/true);
  }

  std::unique_ptr<URLRequestContext> request_context_;
  std::unique_ptr<ResolveContext> resolve_context_;
  std::unique_ptr<DnsClient> dns_client_;
#if BUILDFLAG(IS_ANDROID)
  MockAndroidDnsPlatformAttemptDelegate
      mock_dns_platform_android_attempt_delegate_;
#endif

  MockHostResolverDnsTaskDelegate mock_dns_task_delegate_;
};

// A successful DNS response for www.google.com -> 192.168.1.1.
const std::vector<uint8_t> kSuccessfulDnsResponseA = {
    // Header
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    // Question section
    0x03, 0x77, 0x77, 0x77, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
    0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
    // Answer section
    0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x04,
    0xc0, 0xa8, 0x01, 0x01};

// A successful DNS response for www.google.com -> 2001:0db8::1.
const std::vector<uint8_t> kSuccessfulDnsResponseAaaa = {
    // Header
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    // Question section
    0x03, 0x77, 0x77, 0x77, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
    0x63, 0x6f, 0x6d, 0x00, 0x00, 0x1c, 0x00, 0x01,
    // Answer section
    0xc0, 0x0c, 0x00, 0x1c, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x10,
    0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01};

}  // namespace

#if BUILDFLAG(IS_ANDROID)
TEST_F(HostResolverDnsTaskTest, PlatformAttemptSuccessIsParsedCorrectly) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();
    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                      dns_protocol::kTypeA))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          std::ranges::copy(kSuccessfulDnsResponseA, answer.begin());
          return kSuccessfulDnsResponseA.size();
        });

    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_dns_task_delegate_,
        OnDnsTaskComplete(
            _, /*allow_fallback=*/true, _,
            /*attempt_mode=*/DnsTransactionFactory::AttemptMode::kPlatform))
        .WillOnce([&](base::TimeTicks start_time, bool allow_fallback,
                      HostResolverDnsTask::Results results,
                      DnsTransactionFactory::AttemptMode attempt_mode) {
          EXPECT_THAT(
              results,
              ElementsAre(Pointee(AllOf(
                  Property(&HostResolverInternalResult::query_type,
                           DnsQueryType::A),
                  Property(&HostResolverInternalResult::AsData,
                           Property(&HostResolverInternalDataResult::endpoints,
                                    ElementsAre(IPEndPoint(
                                        IPAddress(192, 168, 1, 1), 0))))))));
          run_loop.Quit();
        });

    base::DefaultTickClock tick_clock;
    DnsQueryTypeSet types = {DnsQueryType::A};
    auto task = std::make_unique<HostResolverDnsTask>(
        dns_client_.get(),
        HostResolver::Host(url::SchemeHostPort(GURL("http://www.google.com"))),
        NetworkAnonymizationKey(), types, resolve_context_.get(),
        DnsTransactionFactory::AttemptMode::kPlatform,
        SecureDnsMode::kAutomatic, handles::kInvalidNetworkHandle,
        &mock_dns_task_delegate_, NetLogWithSource(), &tick_clock,
        /*fallback_available=*/false, HostResolver::HttpsSvcbOptions());
    EXPECT_EQ(task->num_additional_transactions_needed(), 1);
    task->StartNextTransaction();
    // Quit when OnDnsTaskComplete is called.
    run_loop.Run();
    EXPECT_EQ(task->num_additional_transactions_needed(), 0);
  } else {
    GTEST_SKIP_("Skip test on Android version below 29.");
  }
}

TEST_F(HostResolverDnsTaskTest, PlatformAttemptPropagatesTargetNetwork) {
  if (__builtin_available(android 29, *)) {
    constexpr handles::NetworkHandle kTargetNetwork = 123;
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();
    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Query(net_handle_t{kTargetNetwork}, StrEq("www.google.com"),
                      dns_protocol::kTypeA))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          std::ranges::copy(kSuccessfulDnsResponseA, answer.begin());
          return kSuccessfulDnsResponseA.size();
        });

    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_dns_task_delegate_,
        OnDnsTaskComplete(
            _, /*allow_fallback=*/true, _,
            /*attempt_mode=*/DnsTransactionFactory::AttemptMode::kPlatform))
        .WillOnce([&](base::TimeTicks start_time, bool allow_fallback,
                      HostResolverDnsTask::Results results,
                      DnsTransactionFactory::AttemptMode attempt_mode) {
          run_loop.Quit();
        });

    base::DefaultTickClock tick_clock;
    DnsQueryTypeSet types = {DnsQueryType::A};
    auto task = std::make_unique<HostResolverDnsTask>(
        dns_client_.get(),
        HostResolver::Host(url::SchemeHostPort(GURL("http://www.google.com"))),
        NetworkAnonymizationKey(), types, resolve_context_.get(),
        DnsTransactionFactory::AttemptMode::kPlatform,
        SecureDnsMode::kAutomatic, kTargetNetwork, &mock_dns_task_delegate_,
        NetLogWithSource(), &tick_clock,
        /*fallback_available=*/false, HostResolver::HttpsSvcbOptions());
    EXPECT_EQ(task->num_additional_transactions_needed(), 1);
    task->StartNextTransaction();
    run_loop.Run();
    EXPECT_EQ(task->num_additional_transactions_needed(), 0);
  } else {
    GTEST_SKIP_("Skip test on Android version below 29.");
  }
}
TEST_F(HostResolverDnsTaskTest,
       PlatformAttemptCorrectlyTranslatedDefaultNetworkHandle) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();
    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                      dns_protocol::kTypeA))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          std::ranges::copy(kSuccessfulDnsResponseA, answer.begin());
          return kSuccessfulDnsResponseA.size();
        });

    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_dns_task_delegate_,
        OnDnsTaskComplete(
            _, /*allow_fallback=*/true, _,
            /*attempt_mode=*/DnsTransactionFactory::AttemptMode::kPlatform))
        .WillOnce([&](base::TimeTicks start_time, bool allow_fallback,
                      HostResolverDnsTask::Results results,
                      DnsTransactionFactory::AttemptMode attempt_mode) {
          run_loop.Quit();
        });

    base::DefaultTickClock tick_clock;
    DnsQueryTypeSet types = {DnsQueryType::A};
    auto task = std::make_unique<HostResolverDnsTask>(
        dns_client_.get(),
        HostResolver::Host(url::SchemeHostPort(GURL("http://www.google.com"))),
        NetworkAnonymizationKey(), types, resolve_context_.get(),
        DnsTransactionFactory::AttemptMode::kPlatform,
        SecureDnsMode::kAutomatic, handles::kInvalidNetworkHandle,
        &mock_dns_task_delegate_, NetLogWithSource(), &tick_clock,
        /*fallback_available=*/false, HostResolver::HttpsSvcbOptions());
    EXPECT_EQ(task->num_additional_transactions_needed(), 1);
    task->StartNextTransaction();
    run_loop.Run();
    EXPECT_EQ(task->num_additional_transactions_needed(), 0);
  } else {
    GTEST_SKIP_("Skip test on Android version below 29.");
  }
}

TEST_F(HostResolverDnsTaskTest, PlatformAttemptCorruptResponseFailsParsing) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();
    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                      dns_protocol::kTypeA))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          const std::vector<uint8_t> kCorruptDnsResponse = {0x00, 0x01};
          std::ranges::copy(kCorruptDnsResponse, answer.begin());
          return kCorruptDnsResponse.size();
        });

    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_dns_task_delegate_,
        OnDnsTaskComplete(
            _, /*allow_fallback=*/true, _,
            /*attempt_mode=*/DnsTransactionFactory::AttemptMode::kPlatform))
        .WillOnce([&](base::TimeTicks start_time, bool allow_fallback,
                      HostResolverDnsTask::Results results,
                      DnsTransactionFactory::AttemptMode attempt_mode) {
          EXPECT_THAT(
              results,
              ElementsAre(Pointee(AllOf(
                  Property(&HostResolverInternalResult::type,
                           HostResolverInternalResult::Type::kError),
                  Property(&HostResolverInternalResult::AsError,
                           Property(&HostResolverInternalErrorResult::error,
                                    ERR_DNS_MALFORMED_RESPONSE))))));
          run_loop.Quit();
        });

    base::DefaultTickClock tick_clock;
    DnsQueryTypeSet types = {DnsQueryType::A};
    auto task = std::make_unique<HostResolverDnsTask>(
        dns_client_.get(),
        HostResolver::Host(url::SchemeHostPort(GURL("http://www.google.com"))),
        NetworkAnonymizationKey(), types, resolve_context_.get(),
        DnsTransactionFactory::AttemptMode::kPlatform,
        SecureDnsMode::kAutomatic, handles::kInvalidNetworkHandle,
        &mock_dns_task_delegate_, NetLogWithSource(), &tick_clock,
        /*fallback_available=*/false, HostResolver::HttpsSvcbOptions());
    EXPECT_EQ(task->num_additional_transactions_needed(), 1);
    task->StartNextTransaction();
    // Quit when OnDnsTaskComplete is called.
    run_loop.Run();
    EXPECT_EQ(task->num_additional_transactions_needed(), 0);
  } else {
    GTEST_SKIP_("Skip test on Android version below 29.");
  }
}

TEST_F(HostResolverDnsTaskTest,
       PlatformAttemptMultipleQueriesResultsAreSorted) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd_a =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();
    base::ScopedFD fd_aaaa =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();

    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                      dns_protocol::kTypeA))
        .WillOnce(Return(fd_a.get()));
    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                      dns_protocol::kTypeAAAA))
        .WillOnce(Return(fd_aaaa.get()));

    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Result(fd_a.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          std::ranges::copy(kSuccessfulDnsResponseA, answer.begin());
          return kSuccessfulDnsResponseA.size();
        });
    EXPECT_CALL(mock_dns_platform_android_attempt_delegate_,
                Result(fd_aaaa.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          std::ranges::copy(kSuccessfulDnsResponseAaaa, answer.begin());
          return kSuccessfulDnsResponseAaaa.size();
        });

    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_dns_task_delegate_,
        OnDnsTaskComplete(
            _, /*allow_fallback=*/true, _,
            /*attempt_mode=*/DnsTransactionFactory::AttemptMode::kPlatform))
        .WillOnce([&](base::TimeTicks start_time, bool allow_fallback,
                      HostResolverDnsTask::Results results,
                      DnsTransactionFactory::AttemptMode attempt_mode) {
          EXPECT_THAT(
              results,
              ElementsAre(Pointee(AllOf(
                  Property(&HostResolverInternalResult::query_type,
                           DnsQueryType::UNSPECIFIED),
                  Property(
                      &HostResolverInternalResult::AsData,
                      Property(
                          &HostResolverInternalDataResult::endpoints,
                          ElementsAre(
                              IPEndPoint(
                                  IPAddress(0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
                                            0, 0, 0, 0, 0, 0, 0, 1),
                                  0),
                              IPEndPoint(IPAddress(192, 168, 1, 1), 0))))))));
          run_loop.Quit();
        });

    // We do not care about the production behavior of AddressSorter, but we
    // need to ensure that HostResolverDnsTask does end up calling relying on
    // it.
    auto prefer_ipv6_address_sorter = std::make_unique<MockAddressSorter>();
    EXPECT_CALL(*prefer_ipv6_address_sorter, Sort(_, _))
        .WillOnce([](const std::vector<IPEndPoint>& endpoints,
                     AddressSorter::CallbackType callback) {
          EXPECT_THAT(endpoints,
                      UnorderedElementsAre(
                          IPEndPoint(IPAddress(0x20, 0x01, 0x0d, 0xb8, 0, 0, 0,
                                               0, 0, 0, 0, 0, 0, 0, 0, 1),
                                     0),
                          IPEndPoint(IPAddress(192, 168, 1, 1), 0)));
          std::vector<IPEndPoint> sorted_endpoints;
          for (const auto& endpoint : endpoints) {
            if (endpoint.address().IsIPv6()) {
              sorted_endpoints.push_back(endpoint);
            }
          }
          for (const auto& endpoint : endpoints) {
            if (endpoint.address().IsIPv4()) {
              sorted_endpoints.push_back(endpoint);
            }
          }
          std::move(callback).Run(true, std::move(sorted_endpoints));
        });
    dns_client_->SetAddressSorterForTesting(
        std::move(prefer_ipv6_address_sorter));

    base::DefaultTickClock tick_clock;
    DnsQueryTypeSet types = {DnsQueryType::A, DnsQueryType::AAAA};
    auto task = std::make_unique<HostResolverDnsTask>(
        dns_client_.get(),
        HostResolver::Host(url::SchemeHostPort(GURL("http://www.google.com"))),
        NetworkAnonymizationKey(), types, resolve_context_.get(),
        DnsTransactionFactory::AttemptMode::kPlatform,
        SecureDnsMode::kAutomatic, handles::kInvalidNetworkHandle,
        &mock_dns_task_delegate_, NetLogWithSource(), &tick_clock,
        /*fallback_available=*/false, HostResolver::HttpsSvcbOptions());
    EXPECT_EQ(task->num_additional_transactions_needed(), 2);
    task->StartNextTransaction();
    task->StartNextTransaction();
    // Quit when OnDnsTaskComplete is called.
    run_loop.Run();
    EXPECT_EQ(task->num_additional_transactions_needed(), 0);
  } else {
    GTEST_SKIP_("Skip test on Android version below 29.");
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

// Test implementation of AddressSorter that delays calling completion callbacks
// until a call to FinishSorts(). Sorted order is just input order.
class DelayingAddressSorter : public AddressSorter {
 public:
  DelayingAddressSorter() = default;

  void WaitForSortCall() {
    base::RunLoop run_loop;
    on_sort_called_ = run_loop.QuitClosure();
    run_loop.RunUntilIdle();

    CHECK(on_sort_called_.is_null())
        << "DelayingAddressSorter::Sort() not called.";
  }

  void FinishSorts() {
    for (WorkItem& item : in_progress_) {
      std::move(item.callback).Run(/*success=*/true, std::move(item.endpoints));
    }
    in_progress_.clear();
  }

  int NumInProgress() const { return in_progress_.size(); }

  // AddressSorter:
  void Sort(const std::vector<IPEndPoint>& endpoints,
            CallbackType callback) const override {
    in_progress_.emplace_back(endpoints, std::move(callback));

    if (on_sort_called_) {
      std::move(on_sort_called_).Run();
    }
  }

 private:
  struct WorkItem {
    std::vector<IPEndPoint> endpoints;
    CallbackType callback;
  };

  // Mutable to allow test-only functionality from the const Sort() method.
  mutable std::deque<WorkItem> in_progress_;
  mutable base::OnceClosure on_sort_called_;
};

TEST_F(HostResolverDnsTaskTest, HandlesIndividualTransactionSort) {
  constexpr static std::string_view kHost = "foo.test";

  base::test::ScopedFeatureList feature_list(features::kUseHostResolverCache);

  // Configure a mock DnsClient to respond to "foo.test" with 2 addresses per
  // address family.
  DnsResponse a_response = BuildTestDnsResponse(
      std::string(kHost), dns_protocol::kTypeA,
      {BuildTestAddressRecord(std::string(kHost), IPAddress(192, 0, 2, 31)),
       BuildTestAddressRecord(std::string(kHost), IPAddress(192, 0, 2, 36))});
  DnsResponse aaaa_response = BuildTestDnsResponse(
      std::string(kHost), dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord(std::string(kHost),
                              *IPAddress::FromIPLiteral("3fff:14::31")),
       BuildTestAddressRecord(std::string(kHost),
                              *IPAddress::FromIPLiteral("3fff:14::36"))});
  MockDnsClientRuleList rules;
  rules.emplace_back(std::string(kHost), dns_protocol::kTypeA,
                     /*secure=*/false,
                     MockDnsClientRule::Result(std::move(a_response)),
                     /*delay=*/false);
  rules.emplace_back(std::string(kHost), dns_protocol::kTypeAAAA,
                     /*secure=*/false,
                     MockDnsClientRule::Result(std::move(aaaa_response)),
                     /*delay=*/false);
  MockDnsClient mock_dns_client(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client.SetInsecureEnabled(/*enabled=*/true,
                                     /*additional_types_enabled=*/true);

  auto test_sorter = std::make_unique<DelayingAddressSorter>();
  DelayingAddressSorter* test_sorter_ptr = test_sorter.get();
  mock_dns_client.SetAddressSorterForTesting(std::move(test_sorter));

  // Task should wait on sorting before completion.
  EXPECT_CALL(mock_dns_task_delegate_, OnDnsTaskComplete(_, _, _, _)).Times(0);

  base::SimpleTestTickClock clock;
  HostResolverDnsTask task(
      &mock_dns_client,
      HostResolver::Host(url::SchemeHostPort("http", "foo.test", 80)),
      NetworkAnonymizationKey(), {DnsQueryType::A, DnsQueryType::AAAA},
      resolve_context_.get(), DnsTransactionFactory::AttemptMode::kClassic,
      SecureDnsMode::kAutomatic, handles::kInvalidNetworkHandle,
      &mock_dns_task_delegate_, NetLogWithSource(), &clock,
      /*fallback_available=*/false, HostResolver::HttpsSvcbOptions());
  ASSERT_EQ(task.num_additional_transactions_needed(), 2);

  // Expect sort to be called immediately on completion of first transaction.
  task.StartNextTransaction();
  test_sorter_ptr->WaitForSortCall();
  ASSERT_EQ(task.num_additional_transactions_needed(), 1);
  ASSERT_EQ(task.num_transactions_in_progress(), 1);
  ASSERT_EQ(test_sorter_ptr->NumInProgress(), 1);

  // Expect sort to be called immediately on completion of second transaction.
  task.StartNextTransaction();
  test_sorter_ptr->WaitForSortCall();
  ASSERT_EQ(task.num_additional_transactions_needed(), 0);
  ASSERT_EQ(task.num_transactions_in_progress(), 2);
  ASSERT_EQ(test_sorter_ptr->NumInProgress(), 2);

  Mock::VerifyAndClearExpectations(&mock_dns_task_delegate_);
  EXPECT_CALL(mock_dns_task_delegate_, OnDnsTaskComplete(_, _, _, _));
  test_sorter_ptr->FinishSorts();
  EXPECT_EQ(task.num_transactions_in_progress(), 0);
}

TEST_F(HostResolverDnsTaskTest, CanCancelTransactionDuringSort) {
  constexpr static std::string_view kHost = "foo.test";

  base::test::ScopedFeatureList feature_list(features::kUseHostResolverCache);

  // Configure a mock DnsClient to respond to "foo.test" with 2 addresses per
  // address family.
  DnsResponse a_response = BuildTestDnsResponse(
      std::string(kHost), dns_protocol::kTypeA,
      {BuildTestAddressRecord(std::string(kHost), IPAddress(192, 0, 2, 31)),
       BuildTestAddressRecord(std::string(kHost), IPAddress(192, 0, 2, 36))});
  DnsResponse aaaa_response = BuildTestDnsResponse(
      std::string(kHost), dns_protocol::kTypeAAAA,
      {BuildTestAddressRecord(std::string(kHost),
                              *IPAddress::FromIPLiteral("3fff:14::31")),
       BuildTestAddressRecord(std::string(kHost),
                              *IPAddress::FromIPLiteral("3fff:14::36"))});
  MockDnsClientRuleList rules;
  rules.emplace_back(std::string(kHost), dns_protocol::kTypeA,
                     /*secure=*/false,
                     MockDnsClientRule::Result(std::move(a_response)),
                     /*delay=*/false);
  rules.emplace_back(std::string(kHost), dns_protocol::kTypeAAAA,
                     /*secure=*/false,
                     MockDnsClientRule::Result(std::move(aaaa_response)),
                     /*delay=*/false);
  MockDnsClient mock_dns_client(CreateValidDnsConfig(), std::move(rules));
  mock_dns_client.SetInsecureEnabled(/*enabled=*/true,
                                     /*additional_types_enabled=*/true);

  auto test_sorter = std::make_unique<DelayingAddressSorter>();
  DelayingAddressSorter* test_sorter_ptr = test_sorter.get();
  mock_dns_client.SetAddressSorterForTesting(std::move(test_sorter));

  // Task should wait on sorting before completion.
  EXPECT_CALL(mock_dns_task_delegate_, OnDnsTaskComplete(_, _, _, _)).Times(0);

  base::SimpleTestTickClock clock;
  HostResolverDnsTask task(
      &mock_dns_client,
      HostResolver::Host(url::SchemeHostPort("http", "foo.test", 80)),
      NetworkAnonymizationKey(), {DnsQueryType::A, DnsQueryType::AAAA},
      resolve_context_.get(), DnsTransactionFactory::AttemptMode::kClassic,
      SecureDnsMode::kAutomatic, handles::kInvalidNetworkHandle,
      &mock_dns_task_delegate_, NetLogWithSource(), &clock,
      /*fallback_available=*/false, HostResolver::HttpsSvcbOptions());
  ASSERT_EQ(task.num_additional_transactions_needed(), 2);

  // Expect sort to be called immediately on completion of first transaction.
  task.StartNextTransaction();
  test_sorter_ptr->WaitForSortCall();
  ASSERT_EQ(task.num_additional_transactions_needed(), 1);
  ASSERT_EQ(task.num_transactions_in_progress(), 1);
  ASSERT_EQ(test_sorter_ptr->NumInProgress(), 1);

  task.CancelInProgressTransactionsForTest();
  ASSERT_EQ(task.num_additional_transactions_needed(), 1);
  ASSERT_EQ(task.num_transactions_in_progress(), 0);
  ASSERT_EQ(test_sorter_ptr->NumInProgress(), 1);

  test_sorter_ptr->FinishSorts();
  EXPECT_EQ(task.num_additional_transactions_needed(), 1);
  EXPECT_EQ(task.num_transactions_in_progress(), 0);
  EXPECT_EQ(test_sorter_ptr->NumInProgress(), 0);
}

}  // namespace net
