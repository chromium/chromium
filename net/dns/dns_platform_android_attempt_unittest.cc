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
using ::testing::Not;
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
          std::ranges::copy(successful_dns_response, answer.begin());
          return successful_dns_response.size();
        });

    EXPECT_CALL(delegate, Close(fd.get())).Times(0);

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
    // We don't care about the exact errno value. We only want to confirm that
    // DnsPlatformAndroidAttempt correctly reports a failure when
    // android_res_nquery returns a negative value.
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                                dns_protocol::kTypeA))
        .WillOnce(Return(-13));
    EXPECT_CALL(delegate, Result).Times(0);
    EXPECT_CALL(delegate, Close).Times(0);

    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, kQName, dns_protocol::kTypeA,
        handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    EXPECT_THAT(result, IsError(ERR_ACCESS_DENIED));
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
    // We don't care about the exact errno value. We only want to confirm that
    // DnsPlatformAndroidAttempt correctly reports a failure when
    // android_res_nresult returns a negative value.
    EXPECT_CALL(delegate, Result(fd.get(), _, _)).WillOnce(Return(-13));
    EXPECT_CALL(delegate, Close(fd.get())).Times(0);

    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, kQName, dns_protocol::kTypeA,
        handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    EXPECT_THAT(result, IsError(ERR_ACCESS_DENIED));
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
          std::ranges::copy(malformed_dns_response, answer.begin());
          return malformed_dns_response.size();
        });
    EXPECT_CALL(delegate, Close(fd.get())).Times(0);

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
          std::ranges::copy(nxdomain_dns_response, answer.begin());
          return nxdomain_dns_response.size();
        });
    EXPECT_CALL(delegate, Close(fd.get())).Times(0);

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
          std::ranges::copy(truncated_dns_response, answer.begin());
          return truncated_dns_response.size();
        });
    EXPECT_CALL(delegate, Close(fd.get())).Times(0);

    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, kQName, dns_protocol::kTypeA,
        handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    EXPECT_THAT(result, IsError(ERR_UNEXPECTED));
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

// This test simulates a scenario where the DnsPlatformAndroidAttempt is
// destroyed before the response is received (i.e., the file descriptor returned
// by ::Query has not become ready). This should still end up in the file
// descriptor being closed.
// This is a regression test for https://crbug.com/450545129.
TEST_F(DnsPlatformAndroidAttemptTest, DestroyedBeforeResponseClosesFd) {
  if (__builtin_available(android 29, *)) {
    base::ScopedFD fd =
        MockAndroidDnsPlatformAttemptDelegate::CreateFdWithNoData();

    base::RunLoop run_loop;
    MockAndroidDnsPlatformAttemptDelegate delegate;
    EXPECT_CALL(delegate, Query(NETWORK_UNSPECIFIED, StrEq("www.google.com"),
                                dns_protocol::kTypeA))
        .WillOnce([&]() {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(&base::RunLoop::Quit,
                                        base::Unretained(&run_loop)));
          return fd.get();
        });
    EXPECT_CALL(delegate, Result(_, _, _)).Times(0);
    EXPECT_CALL(delegate, Close(fd.get())).WillOnce([&]() {
      EXPECT_EQ(close(fd.release()), 0);
    });

    {
      DnsPlatformAndroidAttempt executor(
          /*server_index=*/0, kQName, dns_protocol::kTypeA,
          handles::kInvalidNetworkHandle, &delegate, NetLogWithSource());

      ResultsCallbackTestFuture future;
      executor.Start(future.GetCallback());
      // Triggers the execution of DnsPlatformAndroidAttempt::StartInternal,
      // which calls ::Query. At this point our mock posts RunLoop::Quit, before
      // returning a valid fd.
      run_loop.Run();
      // The end of the scope destroys the executor, which should close the fd.
    }
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

// E2E test that does not mock the Android API surface
// (DnsPlatformAndroidAttempt::Delegate). Instead, it calls into the real
// Android APIs. We use a non-existing hostname to have a consistent behavior
// that does not rely on the test device internet connectivity.
// This is a regression test for https://crbug.com/450545129.
TEST_F(DnsPlatformAndroidAttemptTest, E2EUnsuccessfulResolution) {
  if (__builtin_available(android 29, *)) {
    // Wire-format DNS name for "we-dont-expect-this-to-resolve.notarealdomain."
    constexpr uint8_t kUnresolvableHostname[] = {
        // Hostname length
        0x1E,
        // [30] we-dont-expect-this-to-resolve
        0x77, 0x65, 0x2D, 0x64, 0x6F, 0x6E, 0x74, 0x2D, 0x65, 0x78, 0x70, 0x65,
        0x63, 0x74, 0x2D, 0x74, 0x68, 0x69, 0x73, 0x2D, 0x74, 0x6F, 0x2D, 0x72,
        0x65, 0x73, 0x6F, 0x6C, 0x76, 0x65,
        // [14] notarealdomain
        0x0E, 0x6E, 0x6F, 0x74, 0x61, 0x72, 0x65, 0x61, 0x6C, 0x64, 0x6F, 0x6D,
        0x61, 0x69, 0x6E,
        // [0] root
        0x00};
    DnsPlatformAndroidAttempt::DelegateImpl delegate;
    DnsPlatformAndroidAttempt executor(
        /*server_index=*/0, base::as_byte_span(kUnresolvableHostname),
        dns_protocol::kTypeA, handles::kInvalidNetworkHandle, &delegate,
        NetLogWithSource());

    ResultsCallbackTestFuture future;
    executor.Start(future.GetCallback());
    int result = future.Take();

    // Make sure it terminates with a failure, but the exact failure is not
    // important.
    EXPECT_NE(result, OK);
    EXPECT_NE(result, ERR_IO_PENDING);
  } else {
    GTEST_SKIP_(kSkipTestOnAndroidVersionBelow29);
  }
}

}  // namespace
}  // namespace net