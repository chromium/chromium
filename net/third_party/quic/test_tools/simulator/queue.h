// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_QUEUE_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_QUEUE_H_

#include "net/third_party/quic/core/quic_alarm.h"
#include "net/third_party/quic/test_tools/simulator/link.h"

namespace quic {
namespace simulator {

// A finitely sized queue which egresses packets onto a constrained link.  The
// capacity of the queue is measured in bytes as opposed to packets.
class Queue : public Actor, public UnconstrainedPortInterface {
 public:
  class ListenerInterface {
   public:
    virtual ~ListenerInterface();

    // Called whenever a packet is removed from the queue.
    virtual void OnPacketDequeued() = 0;
  };

  Queue(Simulator* simulator, QuicString name, QuicByteCount capacity);
  Queue(const Queue&) = delete;
  Queue& operator=(const Queue&) = delete;
  ~Queue() override;

  void set_tx_port(ConstrainedPortInterface* port);

  void AcceptPacket(std::unique_ptr<Packet> packet) override;

  void Act() override;

  inline QuicByteCount capacity() const { return capacity_; }
  inline QuicByteCount bytes_queued() const { return bytes_queued_; }
  inline QuicPacketCount packets_queued() const { return queue_.size(); }

  inline void set_listener_interface(ListenerInterface* listener) {
    listener_ = listener;
  }

  // Enables packet aggregation on the queue.  Packet aggregation makes the
  // queue bundle packets up until they reach certain size.  When the
  // aggregation is enabled, the packets are not dequeued until the total size
  // of packets in the queue reaches |aggregation_threshold|.  The packets are
  // automatically flushed from the queue if the oldest packet has been in it
  // for |aggregation_timeout|.
  //
  // This method may only be called when the queue is empty.  Once enabled,
  // aggregation cannot be disabled.
  void EnableAggregation(QuicByteCount aggregation_threshold,
                         QuicTime::Delta aggregation_timeout);

 private:
  typedef uint64_t AggregationBundleNumber;

  // In order to implement packet aggregation, each packet is tagged with a
  // bundle number.  The queue keeps a bundle counter, and whenever a bundle is
  // ready, it increments the number of the current bundle.  Only the packets
  // outside of the current bundle are allowed to leave the queue.
  struct EnqueuedPacket {
    EnqueuedPacket(std::unique_ptr<Packet> packet,
                   AggregationBundleNumber bundle);
    EnqueuedPacket(EnqueuedPacket&& other);
    ~EnqueuedPacket();

    std::unique_ptr<Packet> packet;
    AggregationBundleNumber bundle;
  };

  // Alarm handler for aggregation timeout.
  class AggregationAlarmDelegate : public QuicAlarm::Delegate {
   public:
    explicit AggregationAlarmDelegate(Queue* queue);

    void OnAlarm() override;

   private:
    Queue* queue_;
  };

  inline bool IsAggregationEnabled() const {
    return aggregation_threshold_ > 0;
  }

  // Increment the bundle counter and reset the bundle state.  This causes all
  // packets currently in the bundle to be flushed onto the link.
  void NextBundle();

  void ScheduleNextPacketDequeue();

  const QuicByteCount capacity_;
  QuicByteCount bytes_queued_;

  QuicByteCount aggregation_threshold_;
  QuicTime::Delta aggregation_timeout_;
  // The number of the current aggregation bundle.  Monotonically increasing.
  // All packets in the previous bundles are allowed to leave the queue, and
  // none of the packets in the current one are.
  AggregationBundleNumber current_bundle_;
  // Size of the current bundle.  Whenever it exceeds |aggregation_threshold_|,
  // the next bundle is created.
  QuicByteCount current_bundle_bytes_;
  // Alarm responsible for flushing the current bundle upon timeout.  Set when
  // the first packet in the bundle is enqueued.
  std::unique_ptr<QuicAlarm> aggregation_timeout_alarm_;

  ConstrainedPortInterface* tx_port_;
  QuicQueue<EnqueuedPacket> queue_;

  ListenerInterface* listener_;
};

}  // namespace simulator
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_SIMULATOR_QUEUE_H_
