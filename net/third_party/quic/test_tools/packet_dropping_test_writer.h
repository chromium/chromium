// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_PACKET_DROPPING_TEST_WRITER_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_PACKET_DROPPING_TEST_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "net/base/ip_address.h"
#include "net/third_party/quic/core/quic_alarm.h"
#include "net/third_party/quic/core/quic_packet_writer_wrapper.h"
#include "net/third_party/quic/platform/impl/quic_epoll_clock.h"
#include "net/third_party/quic/test_tools/quic_test_client.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {

// Simulates a connection that drops packets a configured percentage of the time
// and has a blocked socket a configured percentage of the time.  Also provides
// the options to delay packets and reorder packets if delay is enabled.
class PacketDroppingTestWriter : public QuicPacketWriterWrapper {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnCanWrite() = 0;
  };

  PacketDroppingTestWriter();
  PacketDroppingTestWriter(const PacketDroppingTestWriter&) = delete;
  PacketDroppingTestWriter& operator=(const PacketDroppingTestWriter&) = delete;

  ~PacketDroppingTestWriter() override;

  // Must be called before blocking, reordering or delaying (loss is OK). May be
  // called after connecting if the helper is not available before.
  // |on_can_write| will be triggered when fake-unblocking.
  void Initialize(QuicConnectionHelperInterface* helper,
                  QuicAlarmFactory* alarm_factory,
                  std::unique_ptr<Delegate> on_can_write);

  // QuicPacketWriter methods:
  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          PerPacketOptions* options) override;

  bool IsWriteBlocked() const override;

  void SetWritable() override;

  char* GetNextWriteLocation(const QuicIpAddress& self_address,
                             const QuicSocketAddress& peer_address) override {
    // If the wrapped writer supports zero-copy, disable it, because it is not
    // compatible with delayed writes in this class.
    return nullptr;
  }

  // Writes out any packet which should have been sent by now
  // to the contained writer and returns the time
  // for the next delayed packet to be written.
  QuicTime ReleaseOldPackets();

  // Sets |delay_alarm_| to fire at |new_deadline|.
  void SetDelayAlarm(QuicTime new_deadline);

  void OnCanWrite();

  // The percent of time a packet is simulated as being lost.
  void set_fake_packet_loss_percentage(int32_t fake_packet_loss_percentage);

  // Simulate dropping the first n packets unconditionally.
  // Subsequent packets will be lost at fake_packet_loss_percentage_ if set.
  void set_fake_drop_first_n_packets(int32_t fake_drop_first_n_packets) {
    QuicWriterMutexLock lock(&config_mutex_);
    fake_drop_first_n_packets_ = fake_drop_first_n_packets;
  }

  // The percent of time WritePacket will block and set WriteResult's status
  // to WRITE_STATUS_BLOCKED.
  void set_fake_blocked_socket_percentage(
      int32_t fake_blocked_socket_percentage) {
    DCHECK(clock_);
    QuicWriterMutexLock lock(&config_mutex_);
    fake_blocked_socket_percentage_ = fake_blocked_socket_percentage;
  }

  // The percent of time a packet is simulated as being reordered.
  void set_fake_reorder_percentage(int32_t fake_packet_reorder_percentage) {
    DCHECK(clock_);
    QuicWriterMutexLock lock(&config_mutex_);
    DCHECK(!fake_packet_delay_.IsZero());
    fake_packet_reorder_percentage_ = fake_packet_reorder_percentage;
  }

  // The delay before writing this packet.
  void set_fake_packet_delay(QuicTime::Delta fake_packet_delay) {
    DCHECK(clock_);
    QuicWriterMutexLock lock(&config_mutex_);
    fake_packet_delay_ = fake_packet_delay;
  }

  // The maximum bandwidth and buffer size of the connection.  When these are
  // set, packets will be delayed until a connection with that bandwidth would
  // transmit it.  Once the |buffer_size| is reached, all new packets are
  // dropped.
  void set_max_bandwidth_and_buffer_size(QuicBandwidth fake_bandwidth,
                                         QuicByteCount buffer_size) {
    DCHECK(clock_);
    QuicWriterMutexLock lock(&config_mutex_);
    fake_bandwidth_ = fake_bandwidth;
    buffer_size_ = buffer_size;
  }

  // Useful for reproducing very flaky issues.
  void set_seed(uint64_t seed) { simple_random_.set_seed(seed); }

 private:
  // Writes out the next packet to the contained writer and returns the time
  // for the next delayed packet to be written.
  QuicTime ReleaseNextPacket();

  // A single packet which will be sent at the supplied send_time.
  struct DelayedWrite {
   public:
    DelayedWrite(const char* buffer,
                 size_t buf_len,
                 const QuicIpAddress& self_address,
                 const QuicSocketAddress& peer_address,
                 std::unique_ptr<PerPacketOptions> options,
                 QuicTime send_time);
    DelayedWrite(const DelayedWrite&) = delete;
    DelayedWrite& operator=(const DelayedWrite&) = delete;
    // TODO(rtenneti): on windows RValue reference gives errors.
    DelayedWrite(DelayedWrite&& other);
    // TODO(rtenneti): on windows RValue reference gives errors.
    //    DelayedWrite& operator=(DelayedWrite&& other);
    ~DelayedWrite();

    QuicString buffer;
    const QuicIpAddress self_address;
    const QuicSocketAddress peer_address;
    std::unique_ptr<PerPacketOptions> options;
    QuicTime send_time;
  };

  typedef std::list<DelayedWrite> DelayedPacketList;

  const QuicClock* clock_;
  std::unique_ptr<QuicAlarm> write_unblocked_alarm_;
  std::unique_ptr<QuicAlarm> delay_alarm_;
  std::unique_ptr<Delegate> on_can_write_;
  SimpleRandom simple_random_;
  // Stored packets delayed by fake packet delay or bandwidth restrictions.
  DelayedPacketList delayed_packets_;
  QuicByteCount cur_buffer_size_;
  uint64_t num_calls_to_write_;

  QuicMutex config_mutex_;
  int32_t fake_packet_loss_percentage_ GUARDED_BY(config_mutex_);
  int32_t fake_drop_first_n_packets_ GUARDED_BY(config_mutex_);
  int32_t fake_blocked_socket_percentage_ GUARDED_BY(config_mutex_);
  int32_t fake_packet_reorder_percentage_ GUARDED_BY(config_mutex_);
  QuicTime::Delta fake_packet_delay_ GUARDED_BY(config_mutex_);
  QuicBandwidth fake_bandwidth_ GUARDED_BY(config_mutex_);
  QuicByteCount buffer_size_ GUARDED_BY(config_mutex_);
  int32_t num_consecutive_packet_lost_ GUARDED_BY(config_mutex_);
};

}  // namespace test
}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_PACKET_DROPPING_TEST_WRITER_H_
