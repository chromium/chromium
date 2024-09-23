// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/p2p/base/fake_connection_factory.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "components/webrtc/thread_wrapper.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/webrtc/rtc_base/net_test_helpers.h"
#include "third_party/webrtc_overrides/p2p/base/ice_connection.h"

namespace {
using ::base::test::SingleThreadTaskEnvironment;
using ::base::test::TaskEnvironment;
using ::blink::FakeConnectionFactory;

static const std::string kIpv4Address = "1.1.1.1";
static const std::string kIpv6Address = "2400:4030:1:2c00:be30:abcd:efab:cdef";
constexpr int kPort = 5000;
static const std::string kIpv4AddressString =
    base::StrCat({kIpv4Address, ":", base::NumberToString(kPort)});
static const std::string kIpv6AddressString =
    base::StrCat({"[", kIpv6Address, "]:", base::NumberToString(kPort)});

class FakeConnectionFactoryTest : public ::testing::Test {
 protected:
  FakeConnectionFactoryTest() = default;

  std::unique_ptr<FakeConnectionFactory> GetFactory(bool ipv6 = false) {
    base::WaitableEvent ready(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();
    EXPECT_NE(webrtc::ThreadWrapper::current(), nullptr);

    std::unique_ptr<FakeConnectionFactory> factory =
        std::make_unique<FakeConnectionFactory>(
            webrtc::ThreadWrapper::current(), &ready);

    // Factory doesn't work before initialization.
    EXPECT_EQ(factory->port_count(), 0);
    EXPECT_EQ(factory->CreateConnection(webrtc::IceCandidateType::kHost,
                                        kIpv4Address, kPort),
              nullptr);
    EXPECT_EQ(factory->CreateConnection(webrtc::IceCandidateType::kHost,
                                        kIpv6Address, kPort),
              nullptr);

    int flags = ipv6 ? cricket::PORTALLOCATOR_ENABLE_IPV6 |
                           cricket::PORTALLOCATOR_ENABLE_IPV6_ON_WIFI
                     : cricket::kDefaultPortAllocatorFlags;
    factory->Prepare(flags);
    ready.Wait();

    // A port should have been gathered after initialization is complete.
    EXPECT_GT(factory->port_count(), 0);

    return factory;
  }

  SingleThreadTaskEnvironment env_{TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(FakeConnectionFactoryTest, CreateConnectionIPv4) {
  std::unique_ptr<FakeConnectionFactory> factory = GetFactory();
  const cricket::Connection* conn = factory->CreateConnection(
      webrtc::IceCandidateType::kHost, kIpv4Address, kPort);
  ASSERT_NE(conn, nullptr);
  EXPECT_EQ(conn->remote_candidate().address().ToString(), kIpv4AddressString);
  EXPECT_EQ(conn->network_thread(), webrtc::ThreadWrapper::current());

  // Connection shouldn't be created to an IPv6 remote address if the factory is
  // not initialized for IPv6.
  ASSERT_EQ(factory->CreateConnection(webrtc::IceCandidateType::kHost,
                                      kIpv6Address, kPort),
            nullptr);
}

TEST_F(FakeConnectionFactoryTest, CreateConnectionIPv6) {
  if (rtc::HasIPv6Enabled()) {
    std::unique_ptr<FakeConnectionFactory> factory = GetFactory(/*ipv6=*/true);
    const cricket::Connection* conn = factory->CreateConnection(
        webrtc::IceCandidateType::kHost, kIpv6Address, kPort);
    ASSERT_NE(conn, nullptr);
    EXPECT_EQ(conn->remote_candidate().address().ToString(),
              kIpv6AddressString);
    EXPECT_EQ(conn->network_thread(), webrtc::ThreadWrapper::current());

    // Connection shouldn't be created to an IPv4 remote address if the factory
    // is not initialized for IPv6.
    ASSERT_EQ(factory->CreateConnection(webrtc::IceCandidateType::kHost,
                                        kIpv4Address, kPort),
              nullptr);
  }
}

TEST_F(FakeConnectionFactoryTest, ConvertToIceConnectionIPv4) {
  std::unique_ptr<FakeConnectionFactory> factory = GetFactory();
  const cricket::Connection* conn = factory->CreateConnection(
      webrtc::IceCandidateType::kHost, kIpv4Address, kPort);
  ASSERT_NE(conn, nullptr);
  blink::IceConnection iceConn(conn);
  EXPECT_EQ(iceConn.local_candidate().address().ToString(),
            conn->local_candidate().address().ToString());
  EXPECT_EQ(iceConn.remote_candidate().address().ToString(),
            conn->remote_candidate().address().ToString());
}

TEST_F(FakeConnectionFactoryTest, ConvertToIceConnectionIPv6) {
  if (rtc::HasIPv6Enabled()) {
    std::unique_ptr<FakeConnectionFactory> factory = GetFactory(/*ipv6=*/true);
    const cricket::Connection* conn = factory->CreateConnection(
        webrtc::IceCandidateType::kHost, kIpv6Address, kPort);
    ASSERT_NE(conn, nullptr);
    blink::IceConnection iceConn(conn);
    EXPECT_EQ(iceConn.local_candidate().address().ToString(),
              conn->local_candidate().address().ToString());
    EXPECT_EQ(iceConn.remote_candidate().address().ToString(),
              conn->remote_candidate().address().ToString());
  }
}

}  // unnamed namespace
