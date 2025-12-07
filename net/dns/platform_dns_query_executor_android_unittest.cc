// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/platform_dns_query_executor_android.h"

#include <android/multinetwork.h>
#include <android/versioning.h>
#include <arpa/nameser.h>
#include <stdint.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <memory>
#include <ranges>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/strings/cstring_view.h"
#include "base/test/test_future.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/public/dns_query_type.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;
using ResultsCallbackTestFuture =
    base::test::TestFuture<PlatformDnsQueryExecutorAndroid::Results, int, int>;

static constexpr char kSkipTestOnAndroidVersionBelow29[] =
    "This test is skipped because it's being run on Android 28-, while the "
    "class that it tests is available only on Android 29+.";

base::ScopedFD CreateFdWithUnreadData() {
  std::array<int, 2> fds;
  PCHECK(pipe(fds.data()) == 0);
  base::ScopedFD read_fd(fds[0]);
  base::ScopedFD write_fd(fds[1]);

  std::string_view data = "any data";
  write(write_fd.get(), data.data(), data.size());

  return read_fd;
}

class MockDelegate : public PlatformDnsQueryExecutorAndroid::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(int,
              Query,
              (net_handle_t, base::cstring_view, int, int, uint32_t),
              (override));

  MOCK_METHOD(int, Result, (int, int*, base::span<uint8_t>), (override));
};

class PlatformDnsQueryExecutorAndroidTest : public TestWithTaskEnvironment {};

TEST_F(PlatformDnsQueryExecutorAndroidTest, Success) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd = CreateFdWithUnreadData();

    MockDelegate delegate;

    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                                ns_c_in, ns_t_a, 0))
        .WillOnce(Return(fd.get()));

    EXPECT_CALL(delegate, Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          // A real DNS response for www.google.com -> 192.168.1.1
          const std::vector<uint8_t> dns_response = {
              0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
              0x00, 0x00, 0x03, 0x77, 0x77, 0x77, 0x06, 0x67, 0x6f, 0x6f,
              0x67, 0x6c, 0x65, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01,
              0x00, 0x01, 0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
              0x00, 0x3c, 0x00, 0x04, 0xc0, 0xa8, 0x01, 0x01};

          *rcode = ns_r_noerror;
          CHECK_GE(answer.size(), dns_response.size());
          std::ranges::copy(dns_response, answer.begin());
          return dns_response.size();
        });

    PlatformDnsQueryExecutorAndroid executor(
        "www.google.com", handles::kInvalidNetworkHandle, &delegate);

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    auto [results, os_error, net_error] = future.Take();

    EXPECT_EQ(os_error, 0);
    EXPECT_EQ(net_error, OK);

    ASSERT_THAT(results, ElementsAre(Pointee(Property(
                             &HostResolverInternalResult::type,
                             HostResolverInternalResult::Type::kData))));

    EXPECT_THAT(
        results.begin()->get()->AsData(),
        AllOf(Property(&HostResolverInternalDataResult::query_type,
                       DnsQueryType::A),
              Property(&HostResolverInternalDataResult::endpoints,
                       ElementsAre(Property(
                           &IPEndPoint::address,
                           Property(&IPAddress::ToString, "192.168.1.1"))))));
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

TEST_F(PlatformDnsQueryExecutorAndroidTest,
       FailOnAndroidResNqueryNegativeReturnValue) {
  if (__builtin_available(android 29, *)) {
    MockDelegate delegate;
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("any-domain"),
                                ns_c_in, ns_t_a, 0))
        .WillOnce(Return(-42));
    EXPECT_CALL(delegate, Result).Times(0);

    PlatformDnsQueryExecutorAndroid executor(
        "any-domain", handles::kInvalidNetworkHandle, &delegate);

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    auto [results, os_error, net_error] = future.Take();

    EXPECT_EQ(os_error, 42);
    EXPECT_NE(net_error, OK);
    EXPECT_THAT(results, IsEmpty());
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

TEST_F(PlatformDnsQueryExecutorAndroidTest,
       FailOnAndroidResNresultNegativeReturnValue) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd = CreateFdWithUnreadData();

    MockDelegate delegate;
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("any-domain"),
                                ns_c_in, ns_t_a, 0))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(delegate, Result(fd.get(), _, _)).WillOnce(Return(-42));

    PlatformDnsQueryExecutorAndroid executor(
        "any-domain", handles::kInvalidNetworkHandle, &delegate);

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    auto [results, os_error, net_error] = future.Take();

    EXPECT_EQ(os_error, 42);
    EXPECT_NE(net_error, OK);
    EXPECT_THAT(results, IsEmpty());
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

TEST_F(PlatformDnsQueryExecutorAndroidTest, FailOnAndroidResNresultErrorRcode) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd = CreateFdWithUnreadData();

    MockDelegate delegate;
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("any-domain"),
                                ns_c_in, ns_t_a, 0))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(delegate, Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          *rcode = ns_r_nxdomain;
          return 0;
        });

    PlatformDnsQueryExecutorAndroid executor(
        "any-domain", handles::kInvalidNetworkHandle, &delegate);

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    auto [results, os_error, net_error] = future.Take();

    EXPECT_EQ(os_error, 0);
    EXPECT_EQ(net_error, ERR_NAME_NOT_RESOLVED);
    EXPECT_THAT(results, IsEmpty());
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

}  // namespace
}  // namespace net
