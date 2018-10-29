// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/simulator/simulator.h"

#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/simulator/alarm_factory.h"
#include "net/third_party/quic/test_tools/simulator/link.h"
#include "net/third_party/quic/test_tools/simulator/packet_filter.h"
#include "net/third_party/quic/test_tools/simulator/queue.h"
#include "net/third_party/quic/test_tools/simulator/switch.h"
#include "net/third_party/quic/test_tools/simulator/traffic_policer.h"

using testing::_;
using testing::Return;
using testing::StrictMock;

namespace quic {
namespace simulator {

// A simple counter that increments its value by 1 every specified period.
class Counter : public Actor {
 public:
  Counter(Simulator* simulator, QuicString name, QuicTime::Delta period)
      : Actor(simulator, name), value_(-1), period_(period) {
    Schedule(clock_->Now());
  }
  ~Counter() override {}

  inline int get_value() const { return value_; }

  void Act() override {
    ++value_;
    QUIC_DVLOG(1) << name_ << " has value " << value_ << " at time "
                  << clock_->Now().ToDebuggingValue();
    Schedule(clock_->Now() + period_);
  }

 private:
  int value_;
  QuicTime::Delta period_;
};

class SimulatorTest : public QuicTest {};

// Test that the basic event handling works.
TEST_F(SimulatorTest, Counters) {
  Simulator simulator;
  Counter fast_counter(&simulator, "fast_counter",
                       QuicTime::Delta::FromSeconds(3));
  Counter slow_counter(&simulator, "slow_counter",
                       QuicTime::Delta::FromSeconds(10));

  simulator.RunUntil(
      [&slow_counter]() { return slow_counter.get_value() >= 10; });

  EXPECT_EQ(10, slow_counter.get_value());
  EXPECT_EQ(10 * 10 / 3, fast_counter.get_value());
}

// A port which counts the number of packets received on it, both total and
// per-destination.
class CounterPort : public UnconstrainedPortInterface {
 public:
  CounterPort() { Reset(); }
  ~CounterPort() override {}

  inline QuicByteCount bytes() const { return bytes_; }
  inline QuicPacketCount packets() const { return packets_; }

  void AcceptPacket(std::unique_ptr<Packet> packet) override {
    bytes_ += packet->size;
    packets_ += 1;

    per_destination_packet_counter_[packet->destination] += 1;
  }

  void Reset() {
    bytes_ = 0;
    packets_ = 0;
    per_destination_packet_counter_.clear();
  }

  QuicPacketCount CountPacketsForDestination(QuicString destination) const {
    auto result_it = per_destination_packet_counter_.find(destination);
    if (result_it == per_destination_packet_counter_.cend()) {
      return 0;
    }
    return result_it->second;
  }

 private:
  QuicByteCount bytes_;
  QuicPacketCount packets_;

  QuicUnorderedMap<QuicString, QuicPacketCount> per_destination_packet_counter_;
};

// Sends the packet to the specified destination at the uplink rate.  Provides a
// CounterPort as an Rx interface.
class LinkSaturator : public Endpoint {
 public:
  LinkSaturator(Simulator* simulator,
                QuicString name,
                QuicByteCount packet_size,
                QuicString destination)
      : Endpoint(simulator, name),
        packet_size_(packet_size),
        destination_(std::move(destination)),
        bytes_transmitted_(0),
        packets_transmitted_(0) {
    Schedule(clock_->Now());
  }

  void Act() override {
    if (tx_port_->TimeUntilAvailable().IsZero()) {
      auto packet = QuicMakeUnique<Packet>();
      packet->source = name_;
      packet->destination = destination_;
      packet->tx_timestamp = clock_->Now();
      packet->size = packet_size_;

      tx_port_->AcceptPacket(std::move(packet));

      bytes_transmitted_ += packet_size_;
      packets_transmitted_ += 1;
    }

    Schedule(clock_->Now() + tx_port_->TimeUntilAvailable());
  }

  UnconstrainedPortInterface* GetRxPort() override {
    return static_cast<UnconstrainedPortInterface*>(&rx_port_);
  }

  void SetTxPort(ConstrainedPortInterface* port) override { tx_port_ = port; }

  CounterPort* counter() { return &rx_port_; }

  inline QuicByteCount bytes_transmitted() const { return bytes_transmitted_; }
  inline QuicPacketCount packets_transmitted() const {
    return packets_transmitted_;
  }

  void Pause() { Unschedule(); }
  void Resume() { Schedule(clock_->Now()); }

 private:
  QuicByteCount packet_size_;
  QuicString destination_;

  ConstrainedPortInterface* tx_port_;
  CounterPort rx_port_;

  QuicByteCount bytes_transmitted_;
  QuicPacketCount packets_transmitted_;
};

// Saturate a symmetric link and verify that the number of packets sent and
// received is correct.
TEST_F(SimulatorTest, DirectLinkSaturation) {
  Simulator simulator;
  LinkSaturator saturator_a(&simulator, "Saturator A", 1000, "Saturator B");
  LinkSaturator saturator_b(&simulator, "Saturator B", 100, "Saturator A");
  SymmetricLink link(&saturator_a, &saturator_b,
                     QuicBandwidth::FromKBytesPerSecond(1000),
                     QuicTime::Delta::FromMilliseconds(100) +
                         QuicTime::Delta::FromMicroseconds(1));

  const QuicTime start_time = simulator.GetClock()->Now();
  const QuicTime after_first_50_ms =
      start_time + QuicTime::Delta::FromMilliseconds(50);
  simulator.RunUntil([&simulator, after_first_50_ms]() {
    return simulator.GetClock()->Now() >= after_first_50_ms;
  });
  EXPECT_LE(1000u * 50u, saturator_a.bytes_transmitted());
  EXPECT_GE(1000u * 51u, saturator_a.bytes_transmitted());
  EXPECT_LE(1000u * 50u, saturator_b.bytes_transmitted());
  EXPECT_GE(1000u * 51u, saturator_b.bytes_transmitted());
  EXPECT_LE(50u, saturator_a.packets_transmitted());
  EXPECT_GE(51u, saturator_a.packets_transmitted());
  EXPECT_LE(500u, saturator_b.packets_transmitted());
  EXPECT_GE(501u, saturator_b.packets_transmitted());
  EXPECT_EQ(0u, saturator_a.counter()->bytes());
  EXPECT_EQ(0u, saturator_b.counter()->bytes());

  simulator.RunUntil([&saturator_a, &saturator_b]() {
    if (saturator_a.counter()->packets() > 1000 ||
        saturator_b.counter()->packets() > 100) {
      ADD_FAILURE() << "The simulation did not arrive at the expected "
                       "termination contidition. Saturator A counter: "
                    << saturator_a.counter()->packets()
                    << ", saturator B counter: "
                    << saturator_b.counter()->packets();
      return true;
    }

    return saturator_a.counter()->packets() == 1000 &&
           saturator_b.counter()->packets() == 100;
  });
  EXPECT_EQ(201u, saturator_a.packets_transmitted());
  EXPECT_EQ(2001u, saturator_b.packets_transmitted());
  EXPECT_EQ(201u * 1000, saturator_a.bytes_transmitted());
  EXPECT_EQ(2001u * 100, saturator_b.bytes_transmitted());

  EXPECT_EQ(1000u,
            saturator_a.counter()->CountPacketsForDestination("Saturator A"));
  EXPECT_EQ(100u,
            saturator_b.counter()->CountPacketsForDestination("Saturator B"));
  EXPECT_EQ(0u,
            saturator_a.counter()->CountPacketsForDestination("Saturator B"));
  EXPECT_EQ(0u,
            saturator_b.counter()->CountPacketsForDestination("Saturator A"));

  const QuicTime end_time = simulator.GetClock()->Now();
  const QuicBandwidth observed_bandwidth = QuicBandwidth::FromBytesAndTimeDelta(
      saturator_a.bytes_transmitted(), end_time - start_time);
  test::ExpectApproxEq(link.bandwidth(), observed_bandwidth, 0.01f);
}

// Accepts packets and stores them internally.
class PacketAcceptor : public ConstrainedPortInterface {
 public:
  void AcceptPacket(std::unique_ptr<Packet> packet) override {
    packets_.emplace_back(std::move(packet));
  }

  QuicTime::Delta TimeUntilAvailable() override {
    return QuicTime::Delta::Zero();
  }

  std::vector<std::unique_ptr<Packet>>* packets() { return &packets_; }

 private:
  std::vector<std::unique_ptr<Packet>> packets_;
};

// Ensure the queue behaves correctly with accepting packets.
TEST_F(SimulatorTest, Queue) {
  Simulator simulator;
  Queue queue(&simulator, "Queue", 1000);
  PacketAcceptor acceptor;
  queue.set_tx_port(&acceptor);

  EXPECT_EQ(0u, queue.bytes_queued());
  EXPECT_EQ(0u, queue.packets_queued());
  EXPECT_EQ(0u, acceptor.packets()->size());

  auto first_packet = QuicMakeUnique<Packet>();
  first_packet->size = 600;
  queue.AcceptPacket(std::move(first_packet));
  EXPECT_EQ(600u, queue.bytes_queued());
  EXPECT_EQ(1u, queue.packets_queued());
  EXPECT_EQ(0u, acceptor.packets()->size());

  // The second packet does not fit and is dropped.
  auto second_packet = QuicMakeUnique<Packet>();
  second_packet->size = 500;
  queue.AcceptPacket(std::move(second_packet));
  EXPECT_EQ(600u, queue.bytes_queued());
  EXPECT_EQ(1u, queue.packets_queued());
  EXPECT_EQ(0u, acceptor.packets()->size());

  auto third_packet = QuicMakeUnique<Packet>();
  third_packet->size = 400;
  queue.AcceptPacket(std::move(third_packet));
  EXPECT_EQ(1000u, queue.bytes_queued());
  EXPECT_EQ(2u, queue.packets_queued());
  EXPECT_EQ(0u, acceptor.packets()->size());

  // Run until there is nothing scheduled, so that the queue can deplete.
  simulator.RunUntil([]() { return false; });
  EXPECT_EQ(0u, queue.bytes_queued());
  EXPECT_EQ(0u, queue.packets_queued());
  ASSERT_EQ(2u, acceptor.packets()->size());
  EXPECT_EQ(600u, acceptor.packets()->at(0)->size);
  EXPECT_EQ(400u, acceptor.packets()->at(1)->size);
}

// Simulate a situation where the bottleneck link is 10 times slower than the
// uplink, and they are separated by a queue.
TEST_F(SimulatorTest, QueueBottleneck) {
  const QuicBandwidth local_bandwidth =
      QuicBandwidth::FromKBytesPerSecond(1000);
  const QuicBandwidth bottleneck_bandwidth = 0.1f * local_bandwidth;
  const QuicTime::Delta local_propagation_delay =
      QuicTime::Delta::FromMilliseconds(1);
  const QuicTime::Delta bottleneck_propagation_delay =
      QuicTime::Delta::FromMilliseconds(20);
  const QuicByteCount bdp =
      bottleneck_bandwidth *
      (local_propagation_delay + bottleneck_propagation_delay);

  Simulator simulator;
  LinkSaturator saturator(&simulator, "Saturator", 1000, "Counter");
  ASSERT_GE(bdp, 1000u);
  Queue queue(&simulator, "Queue", bdp);
  CounterPort counter;

  OneWayLink local_link(&simulator, "Local link", &queue, local_bandwidth,
                        local_propagation_delay);
  OneWayLink bottleneck_link(&simulator, "Bottleneck link", &counter,
                             bottleneck_bandwidth,
                             bottleneck_propagation_delay);
  saturator.SetTxPort(&local_link);
  queue.set_tx_port(&bottleneck_link);

  static const QuicPacketCount packets_received = 1000;
  simulator.RunUntil(
      [&counter]() { return counter.packets() == packets_received; });
  const double loss_ratio = 1 - static_cast<double>(packets_received) /
                                    saturator.packets_transmitted();
  EXPECT_NEAR(loss_ratio, 0.9, 0.001);
}

// Verify that the queue of exactly one packet allows the transmission to
// actually go through.
TEST_F(SimulatorTest, OnePacketQueue) {
  const QuicBandwidth local_bandwidth =
      QuicBandwidth::FromKBytesPerSecond(1000);
  const QuicBandwidth bottleneck_bandwidth = 0.1f * local_bandwidth;
  const QuicTime::Delta local_propagation_delay =
      QuicTime::Delta::FromMilliseconds(1);
  const QuicTime::Delta bottleneck_propagation_delay =
      QuicTime::Delta::FromMilliseconds(20);

  Simulator simulator;
  LinkSaturator saturator(&simulator, "Saturator", 1000, "Counter");
  Queue queue(&simulator, "Queue", 1000);
  CounterPort counter;

  OneWayLink local_link(&simulator, "Local link", &queue, local_bandwidth,
                        local_propagation_delay);
  OneWayLink bottleneck_link(&simulator, "Bottleneck link", &counter,
                             bottleneck_bandwidth,
                             bottleneck_propagation_delay);
  saturator.SetTxPort(&local_link);
  queue.set_tx_port(&bottleneck_link);

  static const QuicPacketCount packets_received = 10;
  // The deadline here is to prevent this tests from looping infinitely in case
  // the packets never reach the receiver.
  const QuicTime deadline =
      simulator.GetClock()->Now() + QuicTime::Delta::FromSeconds(10);
  simulator.RunUntil([&simulator, &counter, deadline]() {
    return counter.packets() == packets_received ||
           simulator.GetClock()->Now() > deadline;
  });
  ASSERT_EQ(packets_received, counter.packets());
}

// Simulate a network where three endpoints are connected to a switch and they
// are sending traffic in circle (1 -> 2, 2 -> 3, 3 -> 1).
TEST_F(SimulatorTest, SwitchedNetwork) {
  const QuicBandwidth bandwidth = QuicBandwidth::FromBytesPerSecond(10000);
  const QuicTime::Delta base_propagation_delay =
      QuicTime::Delta::FromMilliseconds(50);

  Simulator simulator;
  LinkSaturator saturator1(&simulator, "Saturator 1", 1000, "Saturator 2");
  LinkSaturator saturator2(&simulator, "Saturator 2", 1000, "Saturator 3");
  LinkSaturator saturator3(&simulator, "Saturator 3", 1000, "Saturator 1");
  Switch network_switch(&simulator, "Switch", 8,
                        bandwidth * base_propagation_delay * 10);

  // For determinicity, make it so that the first packet will arrive from
  // Saturator 1, then from Saturator 2, and then from Saturator 3.
  SymmetricLink link1(&saturator1, network_switch.port(1), bandwidth,
                      base_propagation_delay);
  SymmetricLink link2(&saturator2, network_switch.port(2), bandwidth,
                      base_propagation_delay * 2);
  SymmetricLink link3(&saturator3, network_switch.port(3), bandwidth,
                      base_propagation_delay * 3);

  const QuicTime start_time = simulator.GetClock()->Now();
  static const QuicPacketCount bytes_received = 64 * 1000;
  simulator.RunUntil([&saturator1]() {
    return saturator1.counter()->bytes() >= bytes_received;
  });
  const QuicTime end_time = simulator.GetClock()->Now();

  const QuicBandwidth observed_bandwidth = QuicBandwidth::FromBytesAndTimeDelta(
      bytes_received, end_time - start_time);
  const double bandwidth_ratio =
      static_cast<double>(observed_bandwidth.ToBitsPerSecond()) /
      bandwidth.ToBitsPerSecond();
  EXPECT_NEAR(1, bandwidth_ratio, 0.1);

  const double normalized_received_packets_for_saturator_2 =
      static_cast<double>(saturator2.counter()->packets()) /
      saturator1.counter()->packets();
  const double normalized_received_packets_for_saturator_3 =
      static_cast<double>(saturator3.counter()->packets()) /
      saturator1.counter()->packets();
  EXPECT_NEAR(1, normalized_received_packets_for_saturator_2, 0.1);
  EXPECT_NEAR(1, normalized_received_packets_for_saturator_3, 0.1);

  // Since Saturator 1 has its packet arrive first into the switch, switch will
  // always know how to route traffic to it.
  EXPECT_EQ(0u,
            saturator2.counter()->CountPacketsForDestination("Saturator 1"));
  EXPECT_EQ(0u,
            saturator3.counter()->CountPacketsForDestination("Saturator 1"));

  // Packets from the other saturators will be broadcast at least once.
  EXPECT_EQ(1u,
            saturator1.counter()->CountPacketsForDestination("Saturator 2"));
  EXPECT_EQ(1u,
            saturator3.counter()->CountPacketsForDestination("Saturator 2"));
  EXPECT_EQ(1u,
            saturator1.counter()->CountPacketsForDestination("Saturator 3"));
  EXPECT_EQ(1u,
            saturator2.counter()->CountPacketsForDestination("Saturator 3"));
}

// Toggle an alarm on and off at the specified interval.  Assumes that alarm is
// initially set and unsets it almost immediately after the object is
// instantiated.
class AlarmToggler : public Actor {
 public:
  AlarmToggler(Simulator* simulator,
               QuicString name,
               QuicAlarm* alarm,
               QuicTime::Delta interval)
      : Actor(simulator, name),
        alarm_(alarm),
        interval_(interval),
        deadline_(alarm->deadline()),
        times_set_(0),
        times_cancelled_(0) {
    EXPECT_TRUE(alarm->IsSet());
    EXPECT_GE(alarm->deadline(), clock_->Now());
    Schedule(clock_->Now());
  }

  void Act() override {
    if (deadline_ <= clock_->Now()) {
      return;
    }

    if (alarm_->IsSet()) {
      alarm_->Cancel();
      times_cancelled_++;
    } else {
      alarm_->Set(deadline_);
      times_set_++;
    }

    Schedule(clock_->Now() + interval_);
  }

  inline int times_set() { return times_set_; }
  inline int times_cancelled() { return times_cancelled_; }

 private:
  QuicAlarm* alarm_;
  QuicTime::Delta interval_;
  QuicTime deadline_;

  // Counts the number of times the alarm was set.
  int times_set_;
  // Counts the number of times the alarm was cancelled.
  int times_cancelled_;
};

// Counts the number of times an alarm has fired.
class CounterDelegate : public QuicAlarm::Delegate {
 public:
  explicit CounterDelegate(size_t* counter) : counter_(counter) {}

  void OnAlarm() override { *counter_ += 1; }

 private:
  size_t* counter_;
};

// Verifies that the alarms work correctly, even when they are repeatedly
// toggled.
TEST_F(SimulatorTest, Alarms) {
  Simulator simulator;
  QuicAlarmFactory* alarm_factory = simulator.GetAlarmFactory();

  size_t fast_alarm_counter = 0;
  size_t slow_alarm_counter = 0;
  std::unique_ptr<QuicAlarm> alarm_fast(
      alarm_factory->CreateAlarm(new CounterDelegate(&fast_alarm_counter)));
  std::unique_ptr<QuicAlarm> alarm_slow(
      alarm_factory->CreateAlarm(new CounterDelegate(&slow_alarm_counter)));

  const QuicTime start_time = simulator.GetClock()->Now();
  alarm_fast->Set(start_time + QuicTime::Delta::FromMilliseconds(100));
  alarm_slow->Set(start_time + QuicTime::Delta::FromMilliseconds(750));
  AlarmToggler toggler(&simulator, "Toggler", alarm_slow.get(),
                       QuicTime::Delta::FromMilliseconds(100));

  const QuicTime end_time =
      start_time + QuicTime::Delta::FromMilliseconds(1000);
  EXPECT_FALSE(simulator.RunUntil([&simulator, end_time]() {
    return simulator.GetClock()->Now() >= end_time;
  }));
  EXPECT_EQ(1u, slow_alarm_counter);
  EXPECT_EQ(1u, fast_alarm_counter);

  EXPECT_EQ(4, toggler.times_set());
  EXPECT_EQ(4, toggler.times_cancelled());
}

// Verifies that a cancelled alarm is never fired.
TEST_F(SimulatorTest, AlarmCancelling) {
  Simulator simulator;
  QuicAlarmFactory* alarm_factory = simulator.GetAlarmFactory();

  size_t alarm_counter = 0;
  std::unique_ptr<QuicAlarm> alarm(
      alarm_factory->CreateAlarm(new CounterDelegate(&alarm_counter)));

  const QuicTime start_time = simulator.GetClock()->Now();
  const QuicTime alarm_at = start_time + QuicTime::Delta::FromMilliseconds(300);
  const QuicTime end_time = start_time + QuicTime::Delta::FromMilliseconds(400);

  alarm->Set(alarm_at);
  alarm->Cancel();
  EXPECT_FALSE(alarm->IsSet());

  EXPECT_FALSE(simulator.RunUntil([&simulator, end_time]() {
    return simulator.GetClock()->Now() >= end_time;
  }));

  EXPECT_FALSE(alarm->IsSet());
  EXPECT_EQ(0u, alarm_counter);
}

// Verifies that alarms can be scheduled into the past.
TEST_F(SimulatorTest, AlarmInPast) {
  Simulator simulator;
  QuicAlarmFactory* alarm_factory = simulator.GetAlarmFactory();

  size_t alarm_counter = 0;
  std::unique_ptr<QuicAlarm> alarm(
      alarm_factory->CreateAlarm(new CounterDelegate(&alarm_counter)));

  const QuicTime start_time = simulator.GetClock()->Now();
  simulator.RunFor(QuicTime::Delta::FromMilliseconds(400));

  alarm->Set(start_time);
  simulator.RunFor(QuicTime::Delta::FromMilliseconds(1));
  EXPECT_FALSE(alarm->IsSet());
  EXPECT_EQ(1u, alarm_counter);
}

// Tests Simulator::RunUntilOrTimeout() interface.
TEST_F(SimulatorTest, RunUntilOrTimeout) {
  Simulator simulator;
  bool simulation_result;

  // Count the number of seconds since the beginning of the simulation.
  Counter counter(&simulator, "counter", QuicTime::Delta::FromSeconds(1));

  // Ensure that the counter reaches the value of 10 given a 20 second deadline.
  simulation_result = simulator.RunUntilOrTimeout(
      [&counter]() { return counter.get_value() == 10; },
      QuicTime::Delta::FromSeconds(20));
  ASSERT_TRUE(simulation_result);

  // Ensure that the counter will not reach the value of 100 given that the
  // starting value is 10 and the deadline is 20 seconds.
  simulation_result = simulator.RunUntilOrTimeout(
      [&counter]() { return counter.get_value() == 100; },
      QuicTime::Delta::FromSeconds(20));
  ASSERT_FALSE(simulation_result);
}

// Tests Simulator::RunFor() interface.
TEST_F(SimulatorTest, RunFor) {
  Simulator simulator;

  Counter counter(&simulator, "counter", QuicTime::Delta::FromSeconds(3));

  simulator.RunFor(QuicTime::Delta::FromSeconds(100));

  EXPECT_EQ(33, counter.get_value());
}

class MockPacketFilter : public PacketFilter {
 public:
  MockPacketFilter(Simulator* simulator, QuicString name, Endpoint* endpoint)
      : PacketFilter(simulator, name, endpoint) {}
  MOCK_METHOD1(FilterPacket, bool(const Packet&));
};

// Set up two trivial packet filters, one allowing any packets, and one dropping
// all of them.
TEST_F(SimulatorTest, PacketFilter) {
  const QuicBandwidth bandwidth =
      QuicBandwidth::FromBytesPerSecond(1024 * 1024);
  const QuicTime::Delta base_propagation_delay =
      QuicTime::Delta::FromMilliseconds(5);

  Simulator simulator;
  LinkSaturator saturator_a(&simulator, "Saturator A", 1000, "Saturator B");
  LinkSaturator saturator_b(&simulator, "Saturator B", 1000, "Saturator A");

  // Attach packets to the switch to create a delay between the point at which
  // the packet is generated and the point at which it is filtered.  Note that
  // if the saturators were connected directly, the link would be always
  // available for the endpoint which has all of its packets dropped, resulting
  // in saturator looping infinitely.
  Switch network_switch(&simulator, "Switch", 8,
                        bandwidth * base_propagation_delay * 10);
  StrictMock<MockPacketFilter> a_to_b_filter(&simulator, "A -> B filter",
                                             network_switch.port(1));
  StrictMock<MockPacketFilter> b_to_a_filter(&simulator, "B -> A filter",
                                             network_switch.port(2));
  SymmetricLink link_a(&a_to_b_filter, &saturator_b, bandwidth,
                       base_propagation_delay);
  SymmetricLink link_b(&b_to_a_filter, &saturator_a, bandwidth,
                       base_propagation_delay);

  // Allow packets from A to B, but not from B to A.
  EXPECT_CALL(a_to_b_filter, FilterPacket(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(b_to_a_filter, FilterPacket(_)).WillRepeatedly(Return(false));

  // Run the simulation for a while, and expect that only B will receive any
  // packets.
  simulator.RunFor(QuicTime::Delta::FromSeconds(10));
  EXPECT_GE(saturator_b.counter()->packets(), 1u);
  EXPECT_EQ(saturator_a.counter()->packets(), 0u);
}

// Set up a traffic policer in one direction that throttles at 25% of link
// bandwidth, and put two link saturators at each endpoint.
TEST_F(SimulatorTest, TrafficPolicer) {
  const QuicBandwidth bandwidth =
      QuicBandwidth::FromBytesPerSecond(1024 * 1024);
  const QuicTime::Delta base_propagation_delay =
      QuicTime::Delta::FromMilliseconds(5);
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(10);

  Simulator simulator;
  LinkSaturator saturator1(&simulator, "Saturator 1", 1000, "Saturator 2");
  LinkSaturator saturator2(&simulator, "Saturator 2", 1000, "Saturator 1");
  Switch network_switch(&simulator, "Switch", 8,
                        bandwidth * base_propagation_delay * 10);

  static const QuicByteCount initial_burst = 1000 * 10;
  static const QuicByteCount max_bucket_size = 1000 * 100;
  static const QuicBandwidth target_bandwidth = bandwidth * 0.25;
  TrafficPolicer policer(&simulator, "Policer", initial_burst, max_bucket_size,
                         target_bandwidth, network_switch.port(2));

  SymmetricLink link1(&saturator1, network_switch.port(1), bandwidth,
                      base_propagation_delay);
  SymmetricLink link2(&saturator2, &policer, bandwidth, base_propagation_delay);

  // Ensure the initial burst passes without being dropped at all.
  bool simulator_result = simulator.RunUntilOrTimeout(
      [&saturator1]() {
        return saturator1.bytes_transmitted() == initial_burst;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  saturator1.Pause();
  simulator_result = simulator.RunUntilOrTimeout(
      [&saturator2]() {
        return saturator2.counter()->bytes() == initial_burst;
      },
      timeout);
  ASSERT_TRUE(simulator_result);
  saturator1.Resume();

  // Run for some time so that the initial burst is not visible.
  const QuicTime::Delta simulation_time = QuicTime::Delta::FromSeconds(10);
  simulator.RunFor(simulation_time);

  // Ensure we've transmitted the amount of data we expected.
  for (auto* saturator : {&saturator1, &saturator2}) {
    test::ExpectApproxEq(bandwidth * simulation_time,
                         saturator->bytes_transmitted(), 0.01f);
  }

  // Check that only one direction is throttled.
  test::ExpectApproxEq(saturator1.bytes_transmitted() / 4,
                       saturator2.counter()->bytes(), 0.1f);
  test::ExpectApproxEq(saturator2.bytes_transmitted(),
                       saturator1.counter()->bytes(), 0.1f);
}

// Ensure that a larger burst is allowed when the policed saturator exits
// quiescence.
TEST_F(SimulatorTest, TrafficPolicerBurst) {
  const QuicBandwidth bandwidth =
      QuicBandwidth::FromBytesPerSecond(1024 * 1024);
  const QuicTime::Delta base_propagation_delay =
      QuicTime::Delta::FromMilliseconds(5);
  const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(10);

  Simulator simulator;
  LinkSaturator saturator1(&simulator, "Saturator 1", 1000, "Saturator 2");
  LinkSaturator saturator2(&simulator, "Saturator 2", 1000, "Saturator 1");
  Switch network_switch(&simulator, "Switch", 8,
                        bandwidth * base_propagation_delay * 10);

  const QuicByteCount initial_burst = 1000 * 10;
  const QuicByteCount max_bucket_size = 1000 * 100;
  const QuicBandwidth target_bandwidth = bandwidth * 0.25;
  TrafficPolicer policer(&simulator, "Policer", initial_burst, max_bucket_size,
                         target_bandwidth, network_switch.port(2));

  SymmetricLink link1(&saturator1, network_switch.port(1), bandwidth,
                      base_propagation_delay);
  SymmetricLink link2(&saturator2, &policer, bandwidth, base_propagation_delay);

  // Ensure at least one packet is sent on each side.
  bool simulator_result = simulator.RunUntilOrTimeout(
      [&saturator1, &saturator2]() {
        return saturator1.packets_transmitted() > 0 &&
               saturator2.packets_transmitted() > 0;
      },
      timeout);
  ASSERT_TRUE(simulator_result);

  // Wait until the bucket fills up.
  saturator1.Pause();
  saturator2.Pause();
  simulator.RunFor(1.5f * target_bandwidth.TransferTime(max_bucket_size));

  // Send a burst.
  saturator1.Resume();
  simulator.RunFor(bandwidth.TransferTime(max_bucket_size));
  saturator1.Pause();
  simulator.RunFor(2 * base_propagation_delay);

  // Expect the burst to pass without losses.
  test::ExpectApproxEq(saturator1.bytes_transmitted(),
                       saturator2.counter()->bytes(), 0.1f);

  // Expect subsequent traffic to be policed.
  saturator1.Resume();
  simulator.RunFor(QuicTime::Delta::FromSeconds(10));
  test::ExpectApproxEq(saturator1.bytes_transmitted() / 4,
                       saturator2.counter()->bytes(), 0.1f);
}

// Test that the packet aggregation support in queues work.
TEST_F(SimulatorTest, PacketAggregation) {
  // Model network where the delays are dominated by transfer delay.
  const QuicBandwidth bandwidth = QuicBandwidth::FromBytesPerSecond(1000);
  const QuicTime::Delta base_propagation_delay =
      QuicTime::Delta::FromMicroseconds(1);
  const QuicByteCount aggregation_threshold = 1000;
  const QuicTime::Delta aggregation_timeout = QuicTime::Delta::FromSeconds(30);

  Simulator simulator;
  LinkSaturator saturator1(&simulator, "Saturator 1", 10, "Saturator 2");
  LinkSaturator saturator2(&simulator, "Saturator 2", 10, "Saturator 1");
  Switch network_switch(&simulator, "Switch", 8, 10 * aggregation_threshold);

  // Make links with asymmetric propagation delay so that Saturator 2 only
  // receives packets addressed to it.
  SymmetricLink link1(&saturator1, network_switch.port(1), bandwidth,
                      base_propagation_delay);
  SymmetricLink link2(&saturator2, network_switch.port(2), bandwidth,
                      2 * base_propagation_delay);

  // Enable aggregation in 1 -> 2 direction.
  Queue* queue = network_switch.port_queue(2);
  queue->EnableAggregation(aggregation_threshold, aggregation_timeout);

  // Enable aggregation in 2 -> 1 direction in a way that all packets are larger
  // than the threshold, so that aggregation is effectively a no-op.
  network_switch.port_queue(1)->EnableAggregation(5, aggregation_timeout);

  // Fill up the aggregation buffer up to 90% (900 bytes).
  simulator.RunFor(0.9 * bandwidth.TransferTime(aggregation_threshold));
  EXPECT_EQ(0u, saturator2.counter()->bytes());

  // Stop sending, ensure that given a timespan much shorter than timeout, the
  // packets remain in the queue.
  saturator1.Pause();
  saturator2.Pause();
  simulator.RunFor(QuicTime::Delta::FromSeconds(10));
  EXPECT_EQ(0u, saturator2.counter()->bytes());
  EXPECT_EQ(900u, queue->bytes_queued());

  // Ensure that all packets have reached the saturator not affected by
  // aggregation.  Here, 10 extra bytes account for a misrouted packet in the
  // beginning.
  EXPECT_EQ(910u, saturator1.counter()->bytes());

  // Send 500 more bytes.  Since the aggregation threshold is 1000 bytes, and
  // queue already has 900 bytes, 1000 bytes will be send and 400 will be in the
  // queue.
  saturator1.Resume();
  simulator.RunFor(0.5 * bandwidth.TransferTime(aggregation_threshold));
  saturator1.Pause();
  simulator.RunFor(QuicTime::Delta::FromSeconds(10));
  EXPECT_EQ(1000u, saturator2.counter()->bytes());
  EXPECT_EQ(400u, queue->bytes_queued());

  // Actually time out, and cause all of the data to be received.
  simulator.RunFor(aggregation_timeout);
  EXPECT_EQ(1400u, saturator2.counter()->bytes());
  EXPECT_EQ(0u, queue->bytes_queued());

  // Run saturator for a longer time, to ensure that the logic to cancel and
  // reset alarms works correctly.
  saturator1.Resume();
  simulator.RunFor(5.5 * bandwidth.TransferTime(aggregation_threshold));
  saturator1.Pause();
  simulator.RunFor(QuicTime::Delta::FromSeconds(10));
  EXPECT_EQ(6400u, saturator2.counter()->bytes());
  EXPECT_EQ(500u, queue->bytes_queued());

  // Time out again.
  simulator.RunFor(aggregation_timeout);
  EXPECT_EQ(6900u, saturator2.counter()->bytes());
  EXPECT_EQ(0u, queue->bytes_queued());
}

}  // namespace simulator
}  // namespace quic
