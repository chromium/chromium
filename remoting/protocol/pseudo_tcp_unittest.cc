// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pseudo_tcp.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/auto_spanification_helper.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

// Remove unused using declaration
using ::remoting::protocol::PseudoTcp;

static const int kConnectTimeoutMs =
    60000;  // Much higher timeout for mock time
static const int kTransferTimeoutMs = 240000;  // Increased for tests with loss
static const int kBlockSize = 4096;

class PseudoTcpForTest : public remoting::protocol::PseudoTcp {
 public:
  PseudoTcpForTest(remoting::protocol::IPseudoTcpNotify* notify, uint32_t conv)
      : remoting::protocol::PseudoTcp(notify, conv) {}

  bool isReceiveBufferFull() const {
    return remoting::protocol::PseudoTcp::isReceiveBufferFull();
  }

  void disableWindowScale() {
    remoting::protocol::PseudoTcp::disableWindowScale();
  }
};

class PseudoTcpTestBase : public ::testing::Test,
                          public remoting::protocol::IPseudoTcpNotify {
 public:
  PseudoTcpTestBase()
      : local_(this, 1),
        remote_(this, 1),
        have_connected_(false),
        have_disconnected_(false),
        local_mtu_(65535),
        remote_mtu_(65535),
        delay_(0),
        loss_(0) {
    // Set use of the test RNG to get predictable loss patterns. Otherwise,
    // this test would occasionally get really unlucky loss and time out.
    // Note: WebRTC's SetRandomTestMode doesn't exist in Chromium, so we rely
    // on base::RandUint64() for randomness which is already suitable for
    // testing.
  }
  ~PseudoTcpTestBase() override = default;

  // If true, both endpoints will send the "connect" segment simultaneously,
  // rather than `local_` sending it followed by a response from `remote_`.
  // Note that this is what chromoting ends up doing.
  void SetSimultaneousOpen(bool enabled) { simultaneous_open_ = enabled; }
  void SetLocalMtu(int mtu) {
    local_.NotifyMTU(mtu);
    local_mtu_ = mtu;
  }
  void SetRemoteMtu(int mtu) {
    remote_.NotifyMTU(mtu);
    remote_mtu_ = mtu;
  }
  void SetDelay(int delay) { delay_ = delay; }
  void SetLoss(int percent) { loss_ = percent; }
  // Used to cause the initial "connect" segment to be lost, needed for a
  // regression test.
  void DropNextPacket() { drop_next_packet_ = true; }
  void SetOptNagling(bool enable_nagles) {
    local_.SetOption(PseudoTcp::OPT_NODELAY, !enable_nagles);
    remote_.SetOption(PseudoTcp::OPT_NODELAY, !enable_nagles);
  }
  void SetOptAckDelay(int ack_delay) {
    local_.SetOption(PseudoTcp::OPT_ACKDELAY, ack_delay);
    remote_.SetOption(PseudoTcp::OPT_ACKDELAY, ack_delay);
  }
  void SetOptSndBuf(int size) {
    local_.SetOption(PseudoTcp::OPT_SNDBUF, size);
    remote_.SetOption(PseudoTcp::OPT_SNDBUF, size);
  }
  void SetRemoteOptRcvBuf(int size) {
    remote_.SetOption(PseudoTcp::OPT_RCVBUF, size);
  }
  void SetLocalOptRcvBuf(int size) {
    local_.SetOption(PseudoTcp::OPT_RCVBUF, size);
  }
  void DisableRemoteWindowScale() { remote_.disableWindowScale(); }
  void DisableLocalWindowScale() { local_.disableWindowScale(); }

 protected:
  int Connect() {
    int ret = local_.Connect();
    if (ret == 0) {
      UpdateLocalClock();
    }
    if (simultaneous_open_) {
      ret = remote_.Connect();
      if (ret == 0) {
        UpdateRemoteClock();
      }
    }
    return ret;
  }
  void Close() {
    local_.Close(false);
    UpdateLocalClock();
  }

  void OnTcpOpen(PseudoTcp* tcp) override {
    // Consider ourselves connected when the local side gets OnTcpOpen.
    // OnTcpWriteable isn't fired at open, so we trigger it now.
    VLOG(1) << "Opened";
    if (tcp == &local_) {
      have_connected_ = true;
      OnTcpWriteable(tcp);
    }
  }
  // Test derived from the base should override
  //   virtual void OnTcpReadable(PseudoTcp* tcp)
  // and
  //   virtual void OnTcpWritable(PseudoTcp* tcp)
  void OnTcpClosed(PseudoTcp* tcp, uint32_t error) override {
    // Consider ourselves closed when the remote side gets OnTcpClosed.
    // TODO: OnTcpClosed is only ever notified in case of error in
    // the current implementation.  Solicited close is not (yet) supported.
    VLOG(1) << "Closed";
    EXPECT_EQ(0U, error);
    if (tcp == &remote_) {
      have_disconnected_ = true;
      if (transfer_complete_callback_) {
        std::move(transfer_complete_callback_).Run();
      }
    }
  }
  WriteResult TcpWritePacket(PseudoTcp* tcp,
                             const char* buffer,
                             size_t len) override {
    // Drop a packet if the test called DropNextPacket.
    if (drop_next_packet_) {
      drop_next_packet_ = false;
      VLOG(1) << "Dropping packet due to DropNextPacket, size=" << len;
      return WR_SUCCESS;
    }
    // Randomly drop the desired percentage of packets.
    if (base::RandUint64() % 100 < static_cast<uint32_t>(loss_)) {
      VLOG(1) << "Randomly dropping packet, size=" << len;
      return WR_SUCCESS;
    }
    // Also drop packets that are larger than the configured MTU.
    if (len > static_cast<size_t>(std::min(local_mtu_, remote_mtu_))) {
      VLOG(1) << "Dropping packet that exceeds path MTU, size=" << len;
      return WR_SUCCESS;
    }
    PseudoTcp* other;
    if (tcp == &local_) {
      other = &remote_;
    } else {
      other = &local_;
    }
    std::string packet(buffer, len);
    ++packets_in_flight_;

    // Post delayed task using Chromium's task scheduling
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](PseudoTcp* other, std::string packet, PseudoTcpTestBase* test) {
              --test->packets_in_flight_;
              other->NotifyPacket(packet.c_str(), packet.size());
              test->UpdateClock(*other);
            },
            other, std::move(packet), this),
        base::Milliseconds(delay_));
    return WR_SUCCESS;
  }

  void UpdateLocalClock() { local_.NotifyClock(PseudoTcp::Now()); }
  void UpdateRemoteClock() { remote_.NotifyClock(PseudoTcp::Now()); }
  void UpdateClock(PseudoTcp& tcp) { tcp.NotifyClock(PseudoTcp::Now()); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  PseudoTcpForTest local_;
  PseudoTcpForTest remote_;
  std::vector<uint8_t> send_buffer_;
  std::vector<uint8_t> recv_buffer_;
  bool have_connected_;
  bool have_disconnected_;
  int local_mtu_;
  int remote_mtu_;
  int delay_;
  int loss_;
  bool drop_next_packet_ = false;
  bool simultaneous_open_ = false;
  int packets_in_flight_ = 0;
  base::OnceClosure transfer_complete_callback_;
};

class PseudoTcpTest : public PseudoTcpTestBase {
 public:
  void TestTransfer(int size) {
    base::TimeTicks start;
    base::TimeDelta elapsed;
    size_t received;
    // Create some dummy data to send.
    send_buffer_.reserve(size);
    for (int i = 0; i < size; ++i) {
      send_buffer_.push_back(static_cast<uint8_t>(i));
    }
    send_stream_pos_ = 0;
    // Prepare the receive buffer.
    recv_buffer_.reserve(size);
    // Connect and wait until connected.
    start = base::TimeTicks::Now();
    EXPECT_EQ(0, Connect());

    // Wait for connection - use bounded loop instead of time-based
    for (int i = 0; i < kConnectTimeoutMs / 100 && !have_connected_; i++) {
      UpdateLocalClock();
      UpdateRemoteClock();
      task_environment_.FastForwardBy(base::Milliseconds(100));
      if (i % 100 == 0) {
        VLOG(1) << "Connect attempt " << i
                << ", connected: " << have_connected_;
      }
    }
    EXPECT_TRUE(have_connected_)
        << "Failed to connect after " << kConnectTimeoutMs << "ms";

    // Sending will start from OnTcpWriteable and complete when all data has
    // been received.
    base::RunLoop transfer_loop;
    transfer_complete_callback_ = transfer_loop.QuitClosure();

    // Run transfer with periodic clock updates
    for (int i = 0;
         i < kTransferTimeoutMs / 100 &&
         recv_buffer_.size() < send_buffer_.size() && !have_disconnected_;
         i++) {
      UpdateLocalClock();
      UpdateRemoteClock();
      task_environment_.FastForwardBy(base::Milliseconds(100));
      if (i % 100 == 0) {
        VLOG(1) << "Transfer attempt " << i << ", recv: " << recv_buffer_.size()
                << "/" << send_buffer_.size();
      }
    }

    // If not yet complete, wait for completion with timeout
    if (!have_disconnected_) {
      // In mock time environment, we need to continue advancing time
      // while waiting for completion
      bool transfer_complete = false;
      int timeout_iterations = kTransferTimeoutMs / 100;

      for (int i = 0;
           i < timeout_iterations && !have_disconnected_ && !transfer_complete;
           i++) {
        // More frequent clock updates for tests with loss
        UpdateLocalClock();
        UpdateRemoteClock();
        task_environment_.FastForwardBy(base::Milliseconds(50));
        UpdateLocalClock();
        UpdateRemoteClock();
        task_environment_.FastForwardBy(base::Milliseconds(50));

        // Check if transfer is complete
        if (recv_buffer_.size() == send_buffer_.size()) {
          transfer_complete = true;
          OnTcpClosed(&remote_, 0);  // Manually trigger completion
        }

        if (i % 100 == 0) {
          VLOG(1) << "Final transfer attempt " << i
                  << ", recv: " << recv_buffer_.size() << "/"
                  << send_buffer_.size();
        }
      }
    }

    EXPECT_EQ(recv_buffer_.size(), send_buffer_.size());

    // Clean up connection
    Close();

    elapsed = base::TimeTicks::Now() - start;
    received = recv_buffer_.size();
    // Ensure we closed down OK and we got the right data.
    // TODO: Ensure the errors are cleared properly.
    // EXPECT_EQ(0, local_.GetError());
    // EXPECT_EQ(0, remote_.GetError());
    EXPECT_EQ(static_cast<size_t>(size), received);
    EXPECT_EQ(send_buffer_, recv_buffer_);
    VLOG(1) << "Transferred " << received << " bytes in "
            << elapsed.InMilliseconds() << " ms ("
            << size * 8 / elapsed.InMilliseconds() << " Kbps)";
  }

 private:
  // IPseudoTcpNotify interface

  void OnTcpReadable(PseudoTcp* tcp) override {
    // Stream bytes to the recv buffer as they arrive.
    if (tcp == &remote_) {
      ReadData();

      // TODO: OnTcpClosed() is currently only notified on error -
      // there is no on-the-wire equivalent of TCP FIN.
      // So we fake the notification when all the data has been read.
      size_t received = recv_buffer_.size();
      size_t required = send_buffer_.size();
      if (received == required) {
        OnTcpClosed(&remote_, 0);
      }
    }
  }
  void OnTcpWriteable(PseudoTcp* tcp) override {
    // Write bytes from the send buffer when we can.
    // Shut down when we've sent everything.
    if (tcp == &local_) {
      VLOG(1) << "Flow Control Lifted";
      bool done;
      WriteData(&done);
      if (done) {
        Close();
      }
    }
  }

  void ReadData() {
    std::array<char, kBlockSize> block;
    int received;
    do {
      received = remote_.Recv(block.data(), block.size());
      if (received > 0) {
        recv_buffer_.insert(recv_buffer_.end(), block.begin(),
                            block.begin() + received);
        if (recv_buffer_.size() % 50000 == 0) {
          VLOG(1) << "Received: " << recv_buffer_.size();
        }
      }
    } while (received > 0);
  }
  void WriteData(bool* done) {
    int sent;
    std::array<char, kBlockSize> block;
    do {
      size_t tosend = std::min(static_cast<size_t>(kBlockSize),
                               send_buffer_.size() - send_stream_pos_);
      if (tosend > 0) {
        base::as_writable_bytes(base::span(block).first(tosend))
            .copy_from(base::as_bytes(
                base::span(send_buffer_).subspan(send_stream_pos_, tosend)));
        sent = local_.Send(block.data(), tosend);
        UpdateLocalClock();
        if (sent != -1) {
          send_stream_pos_ += sent;
          if (send_stream_pos_ % 50000 == 0) {
            VLOG(1) << "Sent: " << send_stream_pos_;
          }
        } else {
          VLOG(1) << "Flow Controlled";
        }
      } else {
        sent = 0;
        tosend = 0;
      }
    } while (sent > 0);
    *done = (send_stream_pos_ >= send_buffer_.size());
  }

 private:
  size_t send_stream_pos_ = 0;
};

class PseudoTcpTestPingPong : public PseudoTcpTestBase {
 public:
  PseudoTcpTestPingPong()
      : iterations_remaining_(0),
        sender_(nullptr),
        receiver_(nullptr),
        bytes_per_send_(0) {}
  void SetBytesPerSend(int bytes) { bytes_per_send_ = bytes; }
  void TestPingPong(int size, int iterations) {
    base::TimeTicks start;
    base::TimeDelta elapsed;
    iterations_remaining_ = iterations;
    receiver_ = &remote_;
    sender_ = &local_;
    // Create some dummy data to send.
    send_buffer_.reserve(size);
    for (int i = 0; i < size; ++i) {
      send_buffer_.push_back(static_cast<uint8_t>(i));
    }
    send_stream_pos_ = 0;
    // Prepare the receive buffer.
    recv_buffer_.reserve(size);
    // Connect and wait until connected.
    start = base::TimeTicks::Now();
    EXPECT_EQ(0, Connect());

    // Wait for connection
    for (int i = 0; i < kConnectTimeoutMs / 100 && !have_connected_; i++) {
      UpdateLocalClock();
      UpdateRemoteClock();
      task_environment_.FastForwardBy(base::Milliseconds(100));
    }
    EXPECT_TRUE(have_connected_);

    // Sending will start from OnTcpWriteable and stop when the required
    // number of iterations have completed.
    base::RunLoop pingpong_loop;
    transfer_complete_callback_ = pingpong_loop.QuitClosure();

    for (int i = 0; i < kTransferTimeoutMs / 100 && !have_disconnected_ &&
                    iterations_remaining_ > 0;
         i++) {
      UpdateLocalClock();
      UpdateRemoteClock();
      task_environment_.FastForwardBy(base::Milliseconds(100));
      if (i % 100 == 0) {
        VLOG(1) << "PingPong attempt " << i
                << ", iterations_remaining: " << iterations_remaining_
                << ", have_disconnected: " << have_disconnected_;
      }
    }

    // If not yet complete, wait for completion with timeout
    if (!have_disconnected_ && iterations_remaining_ > 0) {
      // Post a timeout task to quit the loop if transfer doesn't complete
      auto timeout_callback = base::BindOnce(
          [](base::RunLoop* loop) { loop->Quit(); }, &pingpong_loop);

      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, std::move(timeout_callback),
          base::Milliseconds(kTransferTimeoutMs));

      pingpong_loop.Run();

      // Check if we can complete anyway after timeout
      if (!have_disconnected_ && iterations_remaining_ > 0) {
        UpdateLocalClock();
        UpdateRemoteClock();
      }
    }

    EXPECT_TRUE(have_disconnected_ || iterations_remaining_ == 0);

    elapsed = base::TimeTicks::Now() - start;
    VLOG(1) << "Performed " << iterations << " pings in "
            << elapsed.InMilliseconds() << " ms";
  }

 private:
  // IPseudoTcpNotify interface

  void OnTcpReadable(PseudoTcp* tcp) override {
    if (tcp != receiver_) {
      LOG(ERROR) << "unexpected OnTcpReadable";
      return;
    }
    // Stream bytes to the recv buffer as they arrive.
    ReadData();
    // If we've received the desired amount of data, rewind things
    // and send it back the other way!
    size_t position = recv_buffer_.size();
    size_t desired = send_buffer_.size();
    if (position == desired) {
      if (receiver_ == &local_ && --iterations_remaining_ == 0) {
        Close();
        // TODO: Fake OnTcpClosed() on the receiver for now.
        have_disconnected_ = true;  // Set this directly for immediate detection
        OnTcpClosed(&remote_, 0);
        return;
      }
      PseudoTcp* tmp = receiver_;
      receiver_ = sender_;
      sender_ = tmp;
      recv_buffer_.clear();
      send_stream_pos_ = 0;
      OnTcpWriteable(sender_);
    }
  }
  void OnTcpWriteable(PseudoTcp* tcp) override {
    if (tcp != sender_) {
      return;
    }
    // Write bytes from the send buffer when we can.
    // Shut down when we've sent everything.
    VLOG(1) << "Flow Control Lifted";
    WriteData();
  }

  void ReadData() {
    std::array<char, kBlockSize> block;
    int received;
    do {
      received = receiver_->Recv(block.data(), block.size());
      if (received > 0) {
        recv_buffer_.insert(recv_buffer_.end(), block.begin(),
                            block.begin() + received);
        if (recv_buffer_.size() % 50000 == 0) {
          VLOG(1) << "Received: " << recv_buffer_.size();
        }
      }
    } while (received > 0);
  }
  void WriteData() {
    int sent;
    std::array<char, kBlockSize> block;
    do {
      size_t tosend = bytes_per_send_
                          ? std::min(static_cast<size_t>(bytes_per_send_),
                                     send_buffer_.size() - send_stream_pos_)
                          : std::min(static_cast<size_t>(kBlockSize),
                                     send_buffer_.size() - send_stream_pos_);
      if (tosend > 0) {
        base::as_writable_bytes(base::span(block).first(tosend))
            .copy_from(base::as_bytes(
                base::span(send_buffer_).subspan(send_stream_pos_, tosend)));
        sent = sender_->Send(block.data(), tosend);
        UpdateLocalClock();
        if (sent != -1) {
          send_stream_pos_ += sent;
          if (send_stream_pos_ % 50000 == 0) {
            VLOG(1) << "Sent: " << send_stream_pos_;
          }
        } else {
          // Flow control - position stays the same
          VLOG(1) << "Flow Controlled";
        }
      } else {
        sent = 0;
      }
    } while (sent > 0);
  }

 private:
  int iterations_remaining_;
  raw_ptr<PseudoTcp> sender_;
  raw_ptr<PseudoTcp> receiver_;
  int bytes_per_send_;
  size_t send_stream_pos_ = 0;
};

// Fill the receiver window until it is full, drain it and then
// fill it with the same amount. This is to test that receiver window
// contracts and enlarges correctly.
class PseudoTcpTestReceiveWindow : public PseudoTcpTestBase {
 public:
  // Not all the data are transfered, `size` just need to be big enough
  // to fill up the receiver window twice.
  void TestTransfer(int size) {
    // Create some dummy data to send.
    send_buffer_.reserve(size);
    for (int i = 0; i < size; ++i) {
      send_buffer_.push_back(static_cast<uint8_t>(i));
    }
    send_stream_pos_ = 0;

    // Prepare the receive buffer.
    recv_buffer_.reserve(size);

    // Connect and wait until connected.
    EXPECT_EQ(0, Connect());
    base::TimeTicks start = base::TimeTicks::Now();
    while (!have_connected_ && (base::TimeTicks::Now() - start) <
                                   base::Milliseconds(kConnectTimeoutMs)) {
      task_environment_.FastForwardBy(base::Milliseconds(10));
    }
    EXPECT_TRUE(have_connected_);

    WriteData();
    for (int i = 0; i < kTransferTimeoutMs / 100 && !have_disconnected_; i++) {
      task_environment_.FastForwardBy(base::Milliseconds(100));
    }
    EXPECT_TRUE(have_disconnected_);

    ASSERT_EQ(2u, send_position_.size());
    ASSERT_EQ(2u, recv_position_.size());

    const size_t estimated_recv_window = EstimateReceiveWindowSize();

    // The difference in consecutive send positions should equal the
    // receive window size or match very closely. This verifies that receive
    // window is open after receiver drained all the data.
    const size_t send_position_diff = send_position_[1] - send_position_[0];
    EXPECT_GE(1024u, estimated_recv_window - send_position_diff);

    // Receiver drained the receive window twice.
    EXPECT_EQ(2 * estimated_recv_window, recv_position_[1]);
  }

  uint32_t EstimateReceiveWindowSize() const {
    return static_cast<uint32_t>(recv_position_[0]);
  }

  uint32_t EstimateSendWindowSize() const {
    return static_cast<uint32_t>(send_position_[0] - recv_position_[0]);
  }

 private:
  // IPseudoTcpNotify interface
  void OnTcpReadable(PseudoTcp* /* tcp */) override {}

  void OnTcpWriteable(PseudoTcp* /* tcp */) override {}

  void ReadUntilIOPending() {
    std::array<char, kBlockSize> block;
    int received;

    do {
      received = remote_.Recv(block.data(), block.size());
      if (received > 0) {
        recv_buffer_.insert(recv_buffer_.end(), block.begin(),
                            block.begin() + received);
        if (recv_buffer_.size() % 50000 == 0) {
          VLOG(1) << "Received: " << recv_buffer_.size();
        }
      }
    } while (received > 0);

    size_t position = recv_buffer_.size();
    recv_position_.push_back(position);

    // Disconnect if we have done two transfers.
    if (recv_position_.size() == 2u) {
      Close();
      OnTcpClosed(&remote_, 0);
    } else {
      WriteData();
    }
  }

  void WriteData() {
    int sent;
    std::array<char, kBlockSize> block;
    do {
      size_t tosend = std::min(static_cast<size_t>(kBlockSize),
                               send_buffer_.size() - send_stream_pos_);
      if (tosend > 0) {
        base::as_writable_bytes(base::span(block).first(tosend))
            .copy_from(base::as_bytes(
                base::span(send_buffer_).subspan(send_stream_pos_, tosend)));
        sent = local_.Send(block.data(), tosend);
        UpdateLocalClock();
        if (sent != -1) {
          send_stream_pos_ += sent;
          if (send_stream_pos_ % 50000 == 0) {
            VLOG(1) << "Sent: " << send_stream_pos_;
          }
        } else {
          VLOG(1) << "Flow Controlled";
        }
      } else {
        sent = 0;
      }
    } while (sent > 0);
    // At this point, we've filled up the available space in the send queue.

    if (packets_in_flight_ > 0) {
      // If there are packet tasks, attempt to continue sending after giving
      // those packets time to process, which should free up the send buffer.
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PseudoTcpTestReceiveWindow::WriteData,
                         base::Unretained(this)),
          base::Milliseconds(10));
    } else {
      if (!remote_.isReceiveBufferFull()) {
        LOG(ERROR) << "This shouldn't happen - the send buffer is full, "
                      "the receive buffer is not, and there are no "
                      "remaining messages to process.";
      }
      size_t position = send_stream_pos_;
      send_position_.push_back(position);

      // Drain the receiver buffer.
      ReadUntilIOPending();
    }
  }

 private:
  size_t send_stream_pos_ = 0;
  std::vector<size_t> send_position_;
  std::vector<size_t> recv_position_;
};

// Basic end-to-end data transfer tests

// Test the normal case of sending data from one side to the other.
TEST_F(PseudoTcpTest, TestSend) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  TestTransfer(1000000);
}

// Test sending data with a 50 ms RTT. Transmission should take longer due
// to a slower ramp-up in send rate.
TEST_F(PseudoTcpTest, TestSendWithDelay) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetDelay(50);
  TestTransfer(1000000);
}

// Test sending data with packet loss. Transmission should take much longer due
// to send back-off when loss occurs.
TEST_F(PseudoTcpTest, TestSendWithLoss) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetLoss(10);
  TestTransfer(100000);  // less data so test runs faster
}

// Test sending data with a 50 ms RTT and 10% packet loss. Transmission should
// take much longer due to send back-off and slower detection of loss.
TEST_F(PseudoTcpTest, TestSendWithDelayAndLoss) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetDelay(50);
  SetLoss(10);
  TestTransfer(100000);  // less data so test runs faster
}

// Test sending data with 10% packet loss and Nagling disabled.  Transmission
// should take about the same time as with Nagling enabled.
TEST_F(PseudoTcpTest, TestSendWithLossAndOptNaglingOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetLoss(10);
  SetOptNagling(false);
  TestTransfer(100000);  // less data so test runs faster
}

// Regression test for bugs.webrtc.org/9208.
//
// This bug resulted in corrupted data if a "connect" segment was received after
// a data segment. This is only possible if:
//
// * The initial "connect" segment is lost, and retransmitted later.
// * Both sides send "connect"s simultaneously, such that the local side thinks
//   a connection is established even before its "connect" has been
//   acknowledged.
// * Nagle algorithm disabled, allowing a data segment to be sent before the
//   "connect" has been acknowledged.
TEST_F(PseudoTcpTest,
       TestSendWhenFirstPacketLostWithOptNaglingOffAndSimultaneousOpen) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  DropNextPacket();
  SetOptNagling(false);
  SetSimultaneousOpen(true);
  TestTransfer(10000);
}

// Test sending data with 10% packet loss and Delayed ACK disabled.
// Transmission should be slightly faster than with it enabled.
TEST_F(PseudoTcpTest, TestSendWithLossAndOptAckDelayOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetLoss(10);
  SetOptAckDelay(0);
  TestTransfer(100000);
}

// Test sending data with 50ms delay and Nagling disabled.
TEST_F(PseudoTcpTest, TestSendWithDelayAndOptNaglingOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetDelay(50);
  SetOptNagling(false);
  TestTransfer(100000);  // less data so test runs faster
}

// Test sending data with 50ms delay and Delayed ACK disabled.
TEST_F(PseudoTcpTest, TestSendWithDelayAndOptAckDelayOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetDelay(50);
  SetOptAckDelay(0);
  TestTransfer(100000);  // less data so test runs faster
}

// Test a large receive buffer with a sender that doesn't support scaling.
TEST_F(PseudoTcpTest, TestSendRemoteNoWindowScale) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetLocalOptRcvBuf(100000);
  DisableRemoteWindowScale();
  TestTransfer(1000000);
}

// Test a large sender-side receive buffer with a receiver that doesn't support
// scaling.
TEST_F(PseudoTcpTest, TestSendLocalNoWindowScale) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(100000);
  DisableLocalWindowScale();
  TestTransfer(1000000);
}

// Test when both sides use window scaling.
TEST_F(PseudoTcpTest, TestSendBothUseWindowScale) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(100000);
  SetLocalOptRcvBuf(100000);
  TestTransfer(1000000);
}

// Test using a large window scale value.
TEST_F(PseudoTcpTest, TestSendLargeInFlight) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(100000);
  SetLocalOptRcvBuf(100000);
  SetOptSndBuf(150000);
  TestTransfer(1000000);
}

TEST_F(PseudoTcpTest, TestSendBothUseLargeWindowScale) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(1000000);
  SetLocalOptRcvBuf(1000000);
  TestTransfer(10000000);
}

// Test using a small receive buffer.
TEST_F(PseudoTcpTest, TestSendSmallReceiveBuffer) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(10000);
  SetLocalOptRcvBuf(10000);
  TestTransfer(1000000);
}

// Test using a very small receive buffer.
TEST_F(PseudoTcpTest, TestSendVerySmallReceiveBuffer) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetRemoteOptRcvBuf(100);
  SetLocalOptRcvBuf(100);
  TestTransfer(100000);
}

// Ping-pong (request/response) tests

// Test sending <= 1x MTU of data in each ping/pong.  Should take <10ms.
TEST_F(PseudoTcpTestPingPong, TestPingPong1xMtu) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  TestPingPong(100, 100);
}

// Test sending 2x-3x MTU of data in each ping/pong.  Should take <10ms.
TEST_F(PseudoTcpTestPingPong, TestPingPong3xMtu) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  TestPingPong(400, 100);
}

// Test sending 1x-2x MTU of data in each ping/pong.
// Should take ~1s, due to interaction between Nagling and Delayed ACK.
TEST_F(PseudoTcpTestPingPong, TestPingPong2xMtu) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  TestPingPong(2000, 5);
}

// Test sending 1x-2x MTU of data in each ping/pong with Delayed ACK off.
// Should take <10ms.
TEST_F(PseudoTcpTestPingPong, TestPingPong2xMtuWithAckDelayOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptAckDelay(0);
  TestPingPong(2000, 100);
}

// Test sending 1x-2x MTU of data in each ping/pong with Nagling off.
// Should take <10ms.
TEST_F(PseudoTcpTestPingPong, TestPingPong2xMtuWithNaglingOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptNagling(false);
  TestPingPong(2000, 5);
}

// Test sending a ping as pair of short (non-full) segments.
// Should take ~1s, due to Delayed ACK interaction with Nagling.
TEST_F(PseudoTcpTestPingPong, TestPingPongShortSegments) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptAckDelay(5000);
  SetBytesPerSend(50);  // i.e. two Send calls per payload
  TestPingPong(100, 5);
}

// Test sending ping as a pair of short (non-full) segments, with Nagling off.
// Should take <10ms.
TEST_F(PseudoTcpTestPingPong, TestPingPongShortSegmentsWithNaglingOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptNagling(false);
  SetBytesPerSend(50);  // i.e. two Send calls per payload
  TestPingPong(100, 5);
}

// Test sending <= 1x MTU of data ping/pong, in two segments, no Delayed ACK.
// Should take ~1s.
TEST_F(PseudoTcpTestPingPong, TestPingPongShortSegmentsWithAckDelayOff) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetBytesPerSend(50);  // i.e. two Send calls per payload
  SetOptAckDelay(0);
  TestPingPong(100, 5);
}

// Test that receive window expands and contract correctly.
TEST_F(PseudoTcpTestReceiveWindow, TestReceiveWindow) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptNagling(false);
  SetOptAckDelay(0);
  TestTransfer(1024 * 1000);
}

// Test setting send window size to a very small value.
TEST_F(PseudoTcpTestReceiveWindow, TestSetVerySmallSendWindowSize) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptNagling(false);
  SetOptAckDelay(0);
  SetOptSndBuf(900);
  TestTransfer(1024 * 1000);
  EXPECT_EQ(900u, EstimateSendWindowSize());
}

// Test setting receive window size to a value other than default.
TEST_F(PseudoTcpTestReceiveWindow, TestSetReceiveWindowSize) {
  SetLocalMtu(1500);
  SetRemoteMtu(1500);
  SetOptNagling(false);
  SetOptAckDelay(0);
  SetRemoteOptRcvBuf(100000);
  SetLocalOptRcvBuf(100000);
  TestTransfer(1024 * 1000);
  EXPECT_EQ(100000u, EstimateReceiveWindowSize());
}

/* Test sending data with mismatched MTUs. We should detect this and reduce
// our packet size accordingly.
// TODO: This doesn't actually work right now. The current code
// doesn't detect if the MTU is set too high on either side.
TEST_F(PseudoTcpTest, TestSendWithMismatchedMtus) {
  SetLocalMtu(1500);
  SetRemoteMtu(1280);
  TestTransfer(1000000);
}
*/
