// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/port_util.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/simple_thread.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(NetUtilTest, SetExplicitlyAllowedPortsTest) {
  const auto valid = std::to_array<std::vector<uint16_t>>({
      {},
      {1},
      {1, 2},
      {1, 2, 3},
      {10, 11, 12, 13},
  });

  for (size_t i = 0; i < std::size(valid); ++i) {
    SetExplicitlyAllowedPorts(valid[i]);
    EXPECT_EQ(i, GetCountOfExplicitlyAllowedPorts());
  }

  SetExplicitlyAllowedPorts({});
  EXPECT_EQ(0u, GetCountOfExplicitlyAllowedPorts());
}

TEST(NetUtilTest, ScopedPortExceptionTest) {
  SetExplicitlyAllowedPorts({});
  EXPECT_FALSE(IsPortAllowedForScheme(25, "smtp"));
  {
    ScopedPortException exception1(25);
    EXPECT_TRUE(IsPortAllowedForScheme(25, "smtp"));
    EXPECT_EQ(1u, GetCountOfExplicitlyAllowedPorts());
    {
      ScopedPortException exception2(25);
      EXPECT_TRUE(IsPortAllowedForScheme(25, "smtp"));
      EXPECT_EQ(2u, GetCountOfExplicitlyAllowedPorts());
    }
    EXPECT_TRUE(IsPortAllowedForScheme(25, "smtp"));
    EXPECT_EQ(1u, GetCountOfExplicitlyAllowedPorts());
  }
  EXPECT_FALSE(IsPortAllowedForScheme(25, "smtp"));
  EXPECT_EQ(0u, GetCountOfExplicitlyAllowedPorts());

  // Resetting explicitly allowed ports while a ScopedPortException is active
  // should clear the exception without crashing on destruction.
  {
    ScopedPortException exception(25);
    EXPECT_TRUE(IsPortAllowedForScheme(25, "smtp"));
    SetExplicitlyAllowedPorts({});
    EXPECT_FALSE(IsPortAllowedForScheme(25, "smtp"));
  }
  EXPECT_EQ(0u, GetCountOfExplicitlyAllowedPorts());

  // Resetting explicitly allowed ports with the same port while a
  // ScopedPortException is active should not drop that port when the
  // exception goes out of scope.
  {
    ScopedPortException exception(25);
    EXPECT_TRUE(IsPortAllowedForScheme(25, "smtp"));
    constexpr uint16_t kAllowedPorts[] = {25, 8080};
    SetExplicitlyAllowedPorts(kAllowedPorts);
    EXPECT_TRUE(IsPortAllowedForScheme(25, "smtp"));
  }
  EXPECT_TRUE(IsPortAllowedForScheme(25, "smtp"));
  EXPECT_EQ(2u, GetCountOfExplicitlyAllowedPorts());

  // A ScopedPortException created after SetExplicitlyAllowedPorts() should
  // still only remove its own entry on destruction.
  {
    ScopedPortException exception(25);
    EXPECT_EQ(3u, GetCountOfExplicitlyAllowedPorts());
  }
  EXPECT_TRUE(IsPortAllowedForScheme(25, "smtp"));
  EXPECT_EQ(2u, GetCountOfExplicitlyAllowedPorts());

  SetExplicitlyAllowedPorts({});
  EXPECT_FALSE(IsPortAllowedForScheme(25, "smtp"));
  EXPECT_EQ(0u, GetCountOfExplicitlyAllowedPorts());
}

TEST(NetUtilTest, RestrictedAbusePortsTest) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kRestrictAbusePorts,
      {{"restrict_ports", "12345,23456,34567"}, {"monitor_ports", "45678"}});
  EXPECT_TRUE(IsPortAllowedForScheme(443, "https"));
  for (int port : {12345, 23456, 34567}) {
    EXPECT_FALSE(IsPortAllowedForScheme(port, "https"));
  }
  EXPECT_TRUE(IsPortAllowedForScheme(45678, "https"));
  histogram_tester.ExpectTotalCount("Net.RestrictedPorts", 4);
  histogram_tester.ExpectBucketCount("Net.RestrictedPorts", 12345, 1);
  histogram_tester.ExpectBucketCount("Net.RestrictedPorts", 23456, 1);
  histogram_tester.ExpectBucketCount("Net.RestrictedPorts", 34567, 1);
  histogram_tester.ExpectBucketCount("Net.RestrictedPorts", 45678, 1);
}

TEST(NetUtilTest, RestrictedAbusePortsLocalhostTest) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kRestrictAbusePortsOnLocalhost,
      {{"localhost_restrict_ports", "12345,23456,34567"}});
  ReloadLocalhostRestrictedPortsForTesting();
  IPAddress public_address(8, 8, 8, 8);
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(IPEndPoint(public_address, 12345)));
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(
      IPEndPoint(ConvertIPv4ToIPv4MappedIPv6(public_address), 12345)));
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(IPEndPoint(public_address, 443)));
  EXPECT_TRUE(
      IsPortAllowedForIpEndpoint(IPEndPoint(IPAddress::IPv4Localhost(), 443)));
  EXPECT_TRUE(
      IsPortAllowedForIpEndpoint(IPEndPoint(IPAddress::IPv6Localhost(), 443)));
  histogram_tester.ExpectTotalCount("Net.RestrictedLocalhostPorts", 0);

  const IPAddress localhost_addresses[] = {
      IPAddress::IPv4Localhost(),
      IPAddress(127, 0, 0, 2),
      IPAddress::IPv4AllZeros(),
      IPAddress::IPv6Localhost(),
      IPAddress::IPv6AllZeros(),
      ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4Localhost()),
      ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4AllZeros()),
  };

  int expected_count = 0;
  for (int port : {12345, 23456, 34567}) {
    for (const auto& address : localhost_addresses) {
      EXPECT_FALSE(IsPortAllowedForIpEndpoint(IPEndPoint(address, port)));
      expected_count++;
    }
  }
  histogram_tester.ExpectTotalCount("Net.RestrictedLocalhostPorts",
                                    expected_count);
  histogram_tester.ExpectBucketCount("Net.RestrictedLocalhostPorts", 12345,
                                     std::size(localhost_addresses));
  histogram_tester.ExpectBucketCount("Net.RestrictedLocalhostPorts", 23456,
                                     std::size(localhost_addresses));
  histogram_tester.ExpectBucketCount("Net.RestrictedLocalhostPorts", 34567,
                                     std::size(localhost_addresses));
}

TEST(NetUtilTest, RestrictedAbusePortsLocalhostTestNoParamSet) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kRestrictAbusePortsOnLocalhost);
  ReloadLocalhostRestrictedPortsForTesting();
  IPAddress public_address(8, 8, 8, 8);
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(IPEndPoint(public_address, 12345)));
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(
      IPEndPoint(ConvertIPv4ToIPv4MappedIPv6(public_address), 12345)));
  EXPECT_TRUE(IsPortAllowedForIpEndpoint(IPEndPoint(public_address, 443)));
  EXPECT_TRUE(
      IsPortAllowedForIpEndpoint(IPEndPoint(IPAddress::IPv4Localhost(), 443)));
  EXPECT_TRUE(
      IsPortAllowedForIpEndpoint(IPEndPoint(IPAddress::IPv6Localhost(), 443)));
  histogram_tester.ExpectTotalCount("Net.RestrictedLocalhostPorts", 0);

  const IPAddress localhost_addresses[] = {
      IPAddress::IPv4Localhost(),
      IPAddress(127, 0, 0, 2),
      IPAddress::IPv4AllZeros(),
      IPAddress::IPv6Localhost(),
      IPAddress::IPv6AllZeros(),
      ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4Localhost()),
      ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4AllZeros()),
  };

  for (int port : {12345, 23456, 34567}) {
    for (const auto& address : localhost_addresses) {
      EXPECT_TRUE(IsPortAllowedForIpEndpoint(IPEndPoint(address, port)));
    }
  }
  histogram_tester.ExpectTotalCount("Net.RestrictedLocalhostPorts", 0);
}

class PortUtilTestThread : public base::SimpleThread {
 public:
  enum class Operation {
    kWriter,
    kReader,
    kException,
  };

  PortUtilTestThread(Operation op,
                     int iterations,
                     base::WaitableEvent* start_event)
      : base::SimpleThread("PortUtilTestThread"),
        op_(op),
        iterations_(iterations),
        start_event_(start_event) {}

  void Run() override {
    start_event_->Wait();
    for (int i = 0; i < iterations_; ++i) {
      switch (op_) {
        case Operation::kWriter: {
          if (i % 2 == 0) {
            const uint16_t sample_ports_a[] = {80, 443, 8080, 8443, 9000, 9001};
            SetExplicitlyAllowedPorts(sample_ports_a);
          } else {
            const uint16_t sample_ports_b[] = {21, 22, 25, 110, 143, 993, 995};
            SetExplicitlyAllowedPorts(sample_ports_b);
          }
          break;
        }
        case Operation::kReader: {
          EXPECT_TRUE(IsPortAllowedForScheme(80, "http"));
          IsPortAllowedForScheme(25, "smtp");
          EXPECT_TRUE(IsPortAllowedForScheme(443, "https"));
          EXPECT_FALSE(IsPortAllowedForScheme(7, "http"));
          EXPECT_TRUE(IsPortAllowedForIpEndpoint(
              IPEndPoint(IPAddress::IPv4Localhost(), 80)));
          IsPortAllowedForIpEndpoint(
              IPEndPoint(IPAddress::IPv4Localhost(), 25));
          GetCountOfExplicitlyAllowedPorts();
          break;
        }
        case Operation::kException: {
          // Port 79 is restricted by default and not in `sample_ports_a` or
          // `sample_ports_b`. Don't assert the return value since concurrent
          // `SetExplicitlyAllowedPorts()` calls can reset active exceptions.
          ScopedPortException exception(79);
          IsPortAllowedForScheme(79, "http");
          break;
        }
      }
    }
  }

 private:
  const Operation op_;
  const int iterations_;
  const raw_ptr<base::WaitableEvent> start_event_;
};

TEST(NetUtilTest, MultiThreadedExplicitlyAllowedPorts) {
  base::WaitableEvent start_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  constexpr int kIterations = 1000;
  std::vector<std::unique_ptr<PortUtilTestThread>> threads;

  for (int i = 0; i < 2; ++i) {
    threads.push_back(std::make_unique<PortUtilTestThread>(
        PortUtilTestThread::Operation::kWriter, kIterations, &start_event));
  }
  for (int i = 0; i < 4; ++i) {
    threads.push_back(std::make_unique<PortUtilTestThread>(
        PortUtilTestThread::Operation::kReader, kIterations, &start_event));
  }
  for (int i = 0; i < 2; ++i) {
    threads.push_back(std::make_unique<PortUtilTestThread>(
        PortUtilTestThread::Operation::kException, kIterations, &start_event));
  }

  for (auto& thread : threads) {
    thread->Start();
  }

  start_event.Signal();

  for (auto& thread : threads) {
    thread->Join();
  }

  // Restore clean state.
  SetExplicitlyAllowedPorts({});
  EXPECT_EQ(0u, GetCountOfExplicitlyAllowedPorts());
}

}  // namespace
}  // namespace net
