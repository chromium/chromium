// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ephemeral_port_randomizer_mac.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(EphemeralPortRandomizerTest, AvoidsRecentlyUsedPorts) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  EphemeralPortRandomizer::PortRange range = {10000, 10002};
  auto randomizer = EphemeralPortRandomizer::CreateForTesting(
      range, base::Seconds(30),
      base::BindRepeating([](uint16_t first, uint16_t last) { return first; }));

  IPEndPoint peer(IPAddress::IPv4Localhost(), 80);
  auto port1 = randomizer->PickPort(peer);
  ASSERT_TRUE(port1.has_value());
  EXPECT_EQ(*port1, 10000u);

  auto port2 = randomizer->PickPort(peer);
  ASSERT_TRUE(port2.has_value());
  EXPECT_EQ(*port2, 10001u);

  auto port3 = randomizer->PickPort(peer);
  ASSERT_TRUE(port3.has_value());
  EXPECT_EQ(*port3, 10002u);

  EXPECT_FALSE(randomizer->PickPort(peer).has_value());

  task_environment.FastForwardBy(base::Seconds(31));
  auto port4 = randomizer->PickPort(peer);
  ASSERT_TRUE(port4.has_value());
  EXPECT_EQ(*port4, 10000u);
}

TEST(EphemeralPortRandomizerTest, RecordPortUseNewPeer) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  EphemeralPortRandomizer::PortRange range = {10000, 10002};
  auto randomizer = EphemeralPortRandomizer::CreateForTesting(
      range, base::Seconds(30),
      base::BindRepeating([](uint16_t first, uint16_t last) { return first; }));

  IPEndPoint peer(IPAddress::IPv4Localhost(), 80);

  // RecordPortUse for a peer not yet in the map should create the entry.
  randomizer->RecordPortUse(peer, 10000);

  // PickPort should now avoid the recorded port.
  auto port = randomizer->PickPort(peer);
  ASSERT_TRUE(port.has_value());
  EXPECT_EQ(*port, 10001u);
}

TEST(EphemeralPortRandomizerTest, RecordPortUseExistingPeer) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  EphemeralPortRandomizer::PortRange range = {10000, 10002};
  auto randomizer = EphemeralPortRandomizer::CreateForTesting(
      range, base::Seconds(30),
      base::BindRepeating([](uint16_t first, uint16_t last) { return first; }));

  IPEndPoint peer(IPAddress::IPv4Localhost(), 80);

  // Pick a port to create the peer entry.
  auto port1 = randomizer->PickPort(peer);
  ASSERT_TRUE(port1.has_value());
  EXPECT_EQ(*port1, 10000u);

  // RecordPortUse for an existing peer should add to the entry.
  randomizer->RecordPortUse(peer, 10001);

  // PickPort should now avoid both ports.
  auto port2 = randomizer->PickPort(peer);
  ASSERT_TRUE(port2.has_value());
  EXPECT_EQ(*port2, 10002u);
}

TEST(EphemeralPortRandomizerTest, CleanupTimerRemovesStalePeerEntries) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  EphemeralPortRandomizer::PortRange range = {10000, 10002};
  auto randomizer = EphemeralPortRandomizer::CreateForTesting(
      range, base::Seconds(30),
      base::BindRepeating([](uint16_t first, uint16_t last) { return first; }));

  IPEndPoint peer1(IPAddress::IPv4Localhost(), 80);
  IPEndPoint peer2(IPAddress::IPv4Localhost(), 443);

  // Record port use for two different peers.
  randomizer->RecordPortUse(peer1, 10000);
  randomizer->RecordPortUse(peer2, 10001);

  // Both peers should have entries and avoid their recorded ports.
  auto port1 = randomizer->PickPort(peer1);
  ASSERT_TRUE(port1.has_value());
  EXPECT_EQ(*port1, 10001u);

  auto port2 = randomizer->PickPort(peer2);
  ASSERT_TRUE(port2.has_value());
  EXPECT_EQ(*port2, 10000u);

  // Advance time past the reuse delay so the cleanup timer fires.
  task_environment.FastForwardBy(base::Seconds(31));

  // After cleanup, stale peer entries should have been removed. New PickPort
  // calls should behave as if the peers are fresh (returning the first port
  // in the range since the rand callback always returns `first`).
  auto port3 = randomizer->PickPort(peer1);
  ASSERT_TRUE(port3.has_value());
  EXPECT_EQ(*port3, 10000u);

  auto port4 = randomizer->PickPort(peer2);
  ASSERT_TRUE(port4.has_value());
  EXPECT_EQ(*port4, 10000u);
}

TEST(EphemeralPortRandomizerTest, CleanupTimerKeepsActivePeerEntries) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  EphemeralPortRandomizer::PortRange range = {10000, 10002};
  auto randomizer = EphemeralPortRandomizer::CreateForTesting(
      range, base::Seconds(30),
      base::BindRepeating([](uint16_t first, uint16_t last) { return first; }));

  IPEndPoint peer1(IPAddress::IPv4Localhost(), 80);
  IPEndPoint peer2(IPAddress::IPv4Localhost(), 443);

  // Record port use for peer1.
  randomizer->RecordPortUse(peer1, 10000);

  // Advance 20 seconds (less than reuse_delay), then record for peer2.
  task_environment.FastForwardBy(base::Seconds(20));
  randomizer->RecordPortUse(peer2, 10001);

  // Advance another 11 seconds (total 31s from peer1's record, 11s from
  // peer2's). The cleanup timer from peer1's RecordPortUse fires at t=30s.
  // peer1's entry should be cleaned up, but peer2's should remain.
  task_environment.FastForwardBy(base::Seconds(11));

  // peer1 should be cleaned up -- gets fresh first port.
  auto port1 = randomizer->PickPort(peer1);
  ASSERT_TRUE(port1.has_value());
  EXPECT_EQ(*port1, 10000u);

  // peer2 should still have 10001 marked as used.
  auto port2 = randomizer->PickPort(peer2);
  ASSERT_TRUE(port2.has_value());
  EXPECT_EQ(*port2, 10000u);
}

}  // namespace
}  // namespace net
