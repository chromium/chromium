// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_platform_android_attempt.h"

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
#include "net/dns/mock_dns_platform_android_attempt_delegate.h"
#include "net/dns/public/dns_query_type.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using test::IsError;
using test::IsOk;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;
using ResultsCallbackTestFuture = base::test::TestFuture<int>;

static constexpr char kSkipTestOnAndroidVersionBelow29[] =
    "This test is skipped because it's being run on Android 28-, while the "
    "class that it tests is available only on Android 29+.";

class DnsPlatformAndroidAttemptTest : public TestWithTaskEnvironment {};

// A successful DNS response for www.google.com -> 192.168.1.1
const std::vector<uint8_t> successful_dns_response = {
    // Header
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    // Question section
    0x03, 0x77, 0x77, 0x77, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
    0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
    // Answer section
    0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x04,
    0xc0, 0xa8, 0x01, 0x01};

// A failed DNS response for www.google.com that indicates NXDOMAIN.
const std::vector<uint8_t> nxdomain_dns_response = {
    // Header (flags changed to 0x81 0x83, answer RRs changed to 0x00 0x00)
    0x00, 0x00, 0x81, 0x83, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Question section (unchanged)
    0x03, 0x77, 0x77, 0x77, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
    0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01};

// A truncated (TC flag set) DNS response for www.google.com.
const std::vector<uint8_t> truncated_dns_response = {
    // Header (flags changed to 0x83 0x80 to set the TC flag)
    0x00, 0x00, 0x83, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    // Question section (unchanged)
    0x03, 0x77, 0x77, 0x77, 0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x03,
    0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
    // Answer section (unchanged, but considered truncated due to the TC flag)
    0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x04,
    0xc0, 0xa8, 0x01, 0x01};

// A malformed DNS response, not long enough to contain a valid header.
const std::vector<uint8_t> malformed_dns_response = {0x12, 0x23, 0x81};

const char kQNameData[] =
    "\x03"
    "www"
    "\x06"
    "google"
    "\x03"
    "com"
    "\x00";
const base::span<const uint8_t> kQName = base::as_byte_span(kQNameData);

TEST_F(DnsPlatformAndroidAttemptTest, Success) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();

    MockAndroidDnsPlatformAttemptDelegate delegate;

    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                                dns_protocol::kTypeA))
        .WillOnce(Return(fd.get()));

    EXPECT_CALL(delegate, Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          *rcode = dns_protocol::kRcodeNOERROR;
          std::ranges::copy(successful_dns_response, answer.begin());
          return successful_dns_response.size();
        });

    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, kQName, dns_protocol::kTypeA,
        handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    EXPECT_THAT(result, IsOk());
    const DnsResponse* response = executor.GetResponse();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->io_buffer()->size(),
              static_cast<int>(successful_dns_response.size()));
    EXPECT_EQ(absl::string_view(response->io_buffer()->data(),
                                response->io_buffer()->size()),
              absl::string_view(
                  reinterpret_cast<const char*>(successful_dns_response.data()),
                  successful_dns_response.size()));
    EXPECT_EQ(response->rcode(), dns_protocol::kRcodeNOERROR);
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

TEST_F(DnsPlatformAndroidAttemptTest,
       FailOnAndroidResNqueryNegativeReturnValue) {
  if (__builtin_available(android 29, *)) {
    MockAndroidDnsPlatformAttemptDelegate delegate;
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                                dns_protocol::kTypeA))
        .WillOnce(Return(-42));
    EXPECT_CALL(delegate, Result).Times(0);

    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, kQName, dns_protocol::kTypeA,
        handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    EXPECT_THAT(result, IsError(ERR_NAME_NOT_RESOLVED));
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

TEST_F(DnsPlatformAndroidAttemptTest,
       FailOnAndroidResNresultNegativeReturnValue) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();

    MockAndroidDnsPlatformAttemptDelegate delegate;
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                                dns_protocol::kTypeA))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(delegate, Result(fd.get(), _, _)).WillOnce(Return(-42));

    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, kQName, dns_protocol::kTypeA,
        handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    EXPECT_THAT(result, IsError(ERR_NAME_NOT_RESOLVED));
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

TEST_F(DnsPlatformAndroidAttemptTest, FailOnAndroidResNresultErrorRcode) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();

    MockAndroidDnsPlatformAttemptDelegate delegate;
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                                dns_protocol::kTypeA))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(delegate, Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          *rcode = dns_protocol::kRcodeNXDOMAIN;
          return 5;
        });

    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, kQName, dns_protocol::kTypeA,
        handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    EXPECT_THAT(result, IsError(ERR_NAME_NOT_RESOLVED));
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

TEST_F(DnsPlatformAndroidAttemptTest, FailOnMalformedDnsResponse) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();

    MockAndroidDnsPlatformAttemptDelegate delegate;
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                                dns_protocol::kTypeA))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(delegate, Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          *rcode = dns_protocol::kRcodeNOERROR;
          std::ranges::copy(malformed_dns_response, answer.begin());
          return malformed_dns_response.size();
        });

    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, kQName, dns_protocol::kTypeA,
        handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    EXPECT_THAT(result, IsError(ERR_DNS_MALFORMED_RESPONSE));
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

TEST_F(DnsPlatformAndroidAttemptTest, FailOnResponseFlagsNxdomain) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();

    MockAndroidDnsPlatformAttemptDelegate delegate;
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                                dns_protocol::kTypeA))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(delegate, Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          *rcode = dns_protocol::kRcodeNOERROR;
          std::ranges::copy(nxdomain_dns_response, answer.begin());
          return nxdomain_dns_response.size();
        });

    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, kQName, dns_protocol::kTypeA,
        handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    EXPECT_THAT(result, IsError(ERR_NAME_NOT_RESOLVED));
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

TEST_F(DnsPlatformAndroidAttemptTest, FailOnResponseTCFlag) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithUnreadData();

    MockAndroidDnsPlatformAttemptDelegate delegate;
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                                dns_protocol::kTypeA))
        .WillOnce(Return(fd.get()));
    EXPECT_CALL(delegate, Result(fd.get(), _, _))
        .WillOnce([&](int, int* rcode, base::span<uint8_t> answer) {
          *rcode = dns_protocol::kRcodeNOERROR;
          std::ranges::copy(truncated_dns_response, answer.begin());
          return truncated_dns_response.size();
        });

    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, kQName, dns_protocol::kTypeA,
        handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    EXPECT_THAT(result, IsError(ERR_DNS_SERVER_REQUIRES_TCP));
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

}  // namespace
}  // namespace net