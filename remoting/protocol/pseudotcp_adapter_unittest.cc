// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/protocol/pseudotcp_adapter.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/webrtc/thread_wrapper.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "remoting/protocol/p2p_datagram_socket.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

namespace {

const int kMessageSize = 1024;
const int kMessages = 100;
const int kTestDataSize = kMessages * kMessageSize;

class RateLimiter {
 public:
  virtual ~RateLimiter() = default;

  // Returns true if the new packet needs to be dropped, false otherwise.
  virtual bool DropNextPacket() = 0;
};

class LeakyBucket : public RateLimiter {
 public:
  // |rate| is in drops per second.
  LeakyBucket(double volume, double rate)
      : volume_(volume),
        rate_(rate),
        level_(0.0),
        last_update_(base::TimeTicks::Now()) {}

  ~LeakyBucket() override = default;

  bool DropNextPacket() override {
    base::TimeTicks now = base::TimeTicks::Now();
    double interval = (now - last_update_).InSecondsF();
    last_update_ = now;
    level_ = level_ + 1.0 - interval * rate_;
    if (level_ > volume_) {
      level_ = volume_;
      return true;
    } else if (level_ < 0.0) {
      level_ = 0.0;
    }
    return false;
  }

 private:
  double volume_;
  double rate_;
  double level_;
  base::TimeTicks last_update_;
};

class FakeSocket : public P2PDatagramSocket {
 public:
  FakeSocket() : rate_limiter_(nullptr), latency_ms_(0) {}
  ~FakeSocket() override = default;

  void AppendInputPacket(const std::vector<char>& data) {
    if (rate_limiter_ && rate_limiter_->DropNextPacket()) {
      return;  // Lose the packet.
    }

    if (!read_callback_.is_null()) {
      int size = std::min(read_buffer_size_, static_cast<int>(data.size()));
      memcpy(read_buffer_->data(), &data[0], data.size());
      net::CompletionRepeatingCallback cb = read_callback_;
      read_callback_.Reset();
      read_buffer_.reset();
      cb.Run(size);
    } else {
      incoming_packets_.push_back(data);
    }
  }

  void Connect(FakeSocket* peer_socket) { peer_socket_ = peer_socket; }

  void set_rate_limiter(RateLimiter* rate_limiter) {
    rate_limiter_ = rate_limiter;
  }

  void set_latency(int latency_ms) { latency_ms_ = latency_ms; }

  // P2PDatagramSocket interface.
  int Recv(const scoped_refptr<net::IOBuffer>& buf,
           int buf_len,
           const net::CompletionRepeatingCallback& callback) override {
    CHECK(read_callback_.is_null());
    CHECK(buf);

    if (incoming_packets_.size() > 0) {
      scoped_refptr<net::IOBuffer> buffer(buf);
      int size =
          std::min(static_cast<int>(incoming_packets_.front().size()), buf_len);
      memcpy(buffer->data(), &*incoming_packets_.front().begin(), size);
      incoming_packets_.pop_front();
      return size;
    } else {
      read_callback_ = callback;
      read_buffer_ = buf;
      read_buffer_size_ = buf_len;
      return net::ERR_IO_PENDING;
    }
  }

  int Send(const scoped_refptr<net::IOBuffer>& buf,
           int buf_len,
           const net::CompletionRepeatingCallback& callback) override {
    DCHECK(buf);
    if (peer_socket_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeSocket::AppendInputPacket,
                         base::Unretained(peer_socket_),
                         std::vector<char>(buf->data(), buf->data() + buf_len)),
          base::Milliseconds(latency_ms_));
    }

    return buf_len;
  }

 private:
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionRepeatingCallback read_callback_;

  base::circular_deque<std::vector<char>> incoming_packets_;

  raw_ptr<FakeSocket, AcrossTasksDanglingUntriaged> peer_socket_;
  raw_ptr<RateLimiter> rate_limiter_;
  int latency_ms_;
};

class TCPChannelTester : public base::RefCountedThreadSafe<TCPChannelTester> {
 public:
  TCPChannelTester(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   P2PStreamSocket* client_socket,
                   P2PStreamSocket* host_socket,
                   raw_ptr<base::RunLoop> loop)
      : task_runner_(std::move(task_runner)),
        host_socket_(host_socket),
        client_socket_(client_socket),
        done_(false),
        write_errors_(0),
        read_errors_(0),
        loop_(loop) {}

  void Start() {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&TCPChannelTester::DoStart, this));
  }

  void CheckResults() {
    EXPECT_EQ(0, write_errors_);
    EXPECT_EQ(0, read_errors_);

    ASSERT_EQ(kTestDataSize + kMessageSize, input_buffer_->capacity());

    output_buffer_->SetOffset(0);
    ASSERT_EQ(kTestDataSize, output_buffer_->size());

    EXPECT_EQ(output_buffer_->span(), input_buffer_->span_before_offset());
  }

 protected:
  virtual ~TCPChannelTester() = default;

  void Done() {
    done_ = true;
    loop_->QuitWhenIdle();
  }

  void DoStart() {
    InitBuffers();
    DoRead();
    DoWrite();
  }

  void InitBuffers() {
    output_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
        base::MakeRefCounted<net::IOBufferWithSize>(kTestDataSize),
        kTestDataSize);
    memset(output_buffer_->data(), 123, kTestDataSize);

    input_buffer_ = base::MakeRefCounted<net::GrowableIOBuffer>();
    // Always keep kMessageSize bytes available at the end of the input buffer.
    input_buffer_->SetCapacity(kMessageSize);
  }

  void DoWrite() {
    int result = 1;
    while (result > 0) {
      if (output_buffer_->BytesRemaining() == 0) {
        break;
      }

      int bytes_to_write =
          std::min(output_buffer_->BytesRemaining(), kMessageSize);
      result = client_socket_->Write(
          output_buffer_.get(), bytes_to_write,
          base::BindOnce(&TCPChannelTester::OnWritten, base::Unretained(this)),
          TRAFFIC_ANNOTATION_FOR_TESTS);
      HandleWriteResult(result);
    }
  }

  void OnWritten(int result) {
    HandleWriteResult(result);
    DoWrite();
  }

  void HandleWriteResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      LOG(ERROR) << "Received error " << result << " when trying to write";
      write_errors_++;
      Done();
    } else if (result > 0) {
      output_buffer_->DidConsume(result);
    }
  }

  void DoRead() {
    int result = 1;
    while (result > 0) {
      input_buffer_->set_offset(input_buffer_->capacity() - kMessageSize);

      result = host_socket_->Read(
          input_buffer_.get(), kMessageSize,
          base::BindOnce(&TCPChannelTester::OnRead, base::Unretained(this)));
      HandleReadResult(result);
    };
  }

  void OnRead(int result) {
    HandleReadResult(result);
    DoRead();
  }

  void HandleReadResult(int result) {
    if (result <= 0 && result != net::ERR_IO_PENDING) {
      if (!done_) {
        LOG(ERROR) << "Received error " << result << " when trying to read";
        read_errors_++;
        Done();
      }
    } else if (result > 0) {
      // Allocate memory for the next read.
      input_buffer_->SetCapacity(input_buffer_->capacity() + result);
      if (input_buffer_->capacity() == kTestDataSize + kMessageSize) {
        Done();
      }
    }
  }

 private:
  friend class base::RefCountedThreadSafe<TCPChannelTester>;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  raw_ptr<P2PStreamSocket> host_socket_;
  raw_ptr<P2PStreamSocket> client_socket_;
  bool done_;

  scoped_refptr<net::DrainableIOBuffer> output_buffer_;
  scoped_refptr<net::GrowableIOBuffer> input_buffer_;

  int write_errors_;
  int read_errors_;

  raw_ptr<base::RunLoop> loop_;
};

class PseudoTcpAdapterTest : public testing::Test {
 protected:
  void SetUp() override {
    webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();

    host_socket_ = new FakeSocket();
    client_socket_ = new FakeSocket();

    host_socket_->Connect(client_socket_);
    client_socket_->Connect(host_socket_);

    host_pseudotcp_ = std::make_unique<PseudoTcpAdapter>(
        base::WrapUnique(host_socket_.get()));
    client_pseudotcp_ = std::make_unique<PseudoTcpAdapter>(
        base::WrapUnique(client_socket_.get()));
  }

  raw_ptr<FakeSocket, AcrossTasksDanglingUntriaged> host_socket_;
  raw_ptr<FakeSocket, AcrossTasksDanglingUntriaged> client_socket_;

  std::unique_ptr<PseudoTcpAdapter> host_pseudotcp_;
  std::unique_ptr<PseudoTcpAdapter> client_pseudotcp_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(PseudoTcpAdapterTest, DataTransfer) {
  net::TestCompletionCallback host_connect_cb;
  net::TestCompletionCallback client_connect_cb;
  base::RunLoop loop;

  net::CompletionOnceCallback rv1 =
      host_pseudotcp_->Connect(host_connect_cb.callback());
  ASSERT_FALSE(rv1);
  net::CompletionOnceCallback rv2 =
      client_pseudotcp_->Connect(client_connect_cb.callback());
  ASSERT_FALSE(rv2);

  EXPECT_EQ(net::OK, host_connect_cb.WaitForResult());
  EXPECT_EQ(net::OK, client_connect_cb.WaitForResult());

  scoped_refptr<TCPChannelTester> tester = new TCPChannelTester(
      base::SingleThreadTaskRunner::GetCurrentDefault(), host_pseudotcp_.get(),
      client_pseudotcp_.get(), &loop);

  tester->Start();
  loop.Run();
  tester->CheckResults();
}

TEST_F(PseudoTcpAdapterTest, LimitedChannel) {
  const int kLatencyMs = 20;
  const int kPacketsPerSecond = 400;
  const int kBurstPackets = 10;
  base::RunLoop loop;
  LeakyBucket host_limiter(kBurstPackets, kPacketsPerSecond);
  host_socket_->set_latency(kLatencyMs);
  host_socket_->set_rate_limiter(&host_limiter);

  LeakyBucket client_limiter(kBurstPackets, kPacketsPerSecond);
  host_socket_->set_latency(kLatencyMs);
  client_socket_->set_rate_limiter(&client_limiter);

  net::TestCompletionCallback host_connect_cb;
  net::TestCompletionCallback client_connect_cb;

  net::CompletionOnceCallback rv1 =
      host_pseudotcp_->Connect(host_connect_cb.callback());
  ASSERT_FALSE(rv1);
  net::CompletionOnceCallback rv2 =
      client_pseudotcp_->Connect(client_connect_cb.callback());
  ASSERT_FALSE(rv2);

  EXPECT_EQ(net::OK, host_connect_cb.WaitForResult());
  EXPECT_EQ(net::OK, client_connect_cb.WaitForResult());

  scoped_refptr<TCPChannelTester> tester = new TCPChannelTester(
      base::SingleThreadTaskRunner::GetCurrentDefault(), host_pseudotcp_.get(),
      client_pseudotcp_.get(), &loop);

  tester->Start();
  loop.Run();
  tester->CheckResults();

  // Drop unowned reference before local goes out of scope.
  client_socket_->set_rate_limiter(nullptr);
}

class DeleteOnConnected {
 public:
  DeleteOnConnected(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                    std::unique_ptr<PseudoTcpAdapter>* adapter,
                    raw_ptr<base::RunLoop> loop)
      : task_runner_(std::move(task_runner)), adapter_(adapter), loop_(loop) {}
  void OnConnected(int error) {
    adapter_->reset();
    loop_->QuitWhenIdle();
  }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  raw_ptr<std::unique_ptr<PseudoTcpAdapter>> adapter_;
  raw_ptr<base::RunLoop> loop_;
};

TEST_F(PseudoTcpAdapterTest, DeleteOnConnected) {
  // This test verifies that deleting the adapter mid-callback doesn't lead
  // to deleted structures being touched as the stack unrolls, so the failure
  // mode is a crash rather than a normal test failure.
  base::RunLoop loop;
  net::TestCompletionCallback client_connect_cb;
  DeleteOnConnected host_delete(
      base::SingleThreadTaskRunner::GetCurrentDefault(), &host_pseudotcp_,
      &loop);

  host_pseudotcp_->Connect(base::BindOnce(&DeleteOnConnected::OnConnected,
                                          base::Unretained(&host_delete)));
  client_pseudotcp_->Connect(client_connect_cb.callback());
  loop.Run();

  ASSERT_EQ(NULL, host_pseudotcp_.get());
}

// Verify that we can send/receive data with the write-waits-for-send
// flag set.
TEST_F(PseudoTcpAdapterTest, WriteWaitsForSendLetsDataThrough) {
  net::TestCompletionCallback host_connect_cb;
  net::TestCompletionCallback client_connect_cb;

  base::RunLoop loop;
  host_pseudotcp_->SetWriteWaitsForSend(true);
  client_pseudotcp_->SetWriteWaitsForSend(true);

  // Disable Nagle's algorithm because the test is slow when it is
  // enabled.
  host_pseudotcp_->SetNoDelay(true);

  net::CompletionOnceCallback rv1 =
      host_pseudotcp_->Connect(host_connect_cb.callback());
  ASSERT_FALSE(rv1);
  net::CompletionOnceCallback rv2 =
      client_pseudotcp_->Connect(client_connect_cb.callback());
  ASSERT_FALSE(rv2);

  EXPECT_EQ(net::OK, host_connect_cb.WaitForResult());
  EXPECT_EQ(net::OK, client_connect_cb.WaitForResult());

  scoped_refptr<TCPChannelTester> tester = new TCPChannelTester(
      base::SingleThreadTaskRunner::GetCurrentDefault(), host_pseudotcp_.get(),
      client_pseudotcp_.get(), &loop);

  tester->Start();
  loop.Run();
  tester->CheckResults();
}

}  // namespace

}  // namespace remoting::protocol
