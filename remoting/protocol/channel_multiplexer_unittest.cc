// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/protocol/channel_multiplexer.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtMost;
using testing::InvokeWithoutArgs;

namespace remoting::protocol {

namespace {

const int kMessageSize = 1024;
const int kMessages = 100;
const char kMuxChannelName[] = "mux";

const char kTestChannelName[] = "test";
const char kTestChannelName2[] = "test2";

class MockConnectCallback {
 public:
  MOCK_METHOD1(OnConnectedPtr, void(P2PStreamSocket* socket));
  void OnConnected(std::unique_ptr<P2PStreamSocket> socket) {
    OnConnectedPtr(socket.release());
  }
};

}  // namespace

class ChannelMultiplexerTest : public testing::Test {
 public:
  void DeleteAll() {
    host_socket1_.reset();
    host_socket2_.reset();
    client_socket1_.reset();
    client_socket2_.reset();
    host_mux_.reset();
    client_mux_.reset();
  }

  void DeleteAfterSessionFail() {
    host_mux_->CancelChannelCreation(kTestChannelName2);
    DeleteAll();
  }

 protected:
  void SetUp() override {
    host_channel_factory_.PairWith(&client_channel_factory_);

    // Create pair of multiplexers and connect them to each other.
    host_mux_ = std::make_unique<ChannelMultiplexer>(&host_channel_factory_,
                                                     kMuxChannelName);
    client_mux_ = std::make_unique<ChannelMultiplexer>(&client_channel_factory_,
                                                       kMuxChannelName);

    // Make writes asynchronous in one direction
    host_channel_factory_.set_async_write(true);
  }

  void CreateChannel(const std::string& name,
                     std::unique_ptr<P2PStreamSocket>* host_socket,
                     std::unique_ptr<P2PStreamSocket>* client_socket) {
    int counter = 2;
    base::RunLoop loop;
    host_mux_->CreateChannel(
        name, base::BindOnce(&ChannelMultiplexerTest::OnChannelConnected,
                             base::Unretained(this), host_socket, &counter,
                             loop.QuitWhenIdleClosure()));
    client_mux_->CreateChannel(
        name, base::BindOnce(&ChannelMultiplexerTest::OnChannelConnected,
                             base::Unretained(this), client_socket, &counter,
                             loop.QuitWhenIdleClosure()));

    loop.Run();

    EXPECT_TRUE(host_socket->get());
    EXPECT_TRUE(client_socket->get());
  }

  void OnChannelConnected(std::unique_ptr<P2PStreamSocket>* storage,
                          int* counter,
                          base::OnceClosure quit_closure,
                          std::unique_ptr<P2PStreamSocket> socket) {
    *storage = std::move(socket);
    --(*counter);
    EXPECT_GE(*counter, 0);
    if (*counter == 0) {
      std::move(quit_closure).Run();
    }
  }

  scoped_refptr<net::IOBufferWithSize> CreateTestBuffer(int size) {
    scoped_refptr<net::IOBufferWithSize> result =
        base::MakeRefCounted<net::IOBufferWithSize>(size);
    for (int i = 0; i < size; ++i) {
      result->data()[i] = rand() % 256;
    }
    return result;
  }

 private:
  // Must be instantiated before the FakeStreamChannelFactories below.
  base::test::SingleThreadTaskEnvironment task_environment_;

 protected:
  FakeStreamChannelFactory host_channel_factory_;
  FakeStreamChannelFactory client_channel_factory_;

  std::unique_ptr<ChannelMultiplexer> host_mux_;
  std::unique_ptr<ChannelMultiplexer> client_mux_;

  std::unique_ptr<P2PStreamSocket> host_socket1_;
  std::unique_ptr<P2PStreamSocket> client_socket1_;
  std::unique_ptr<P2PStreamSocket> host_socket2_;
  std::unique_ptr<P2PStreamSocket> client_socket2_;
};

TEST_F(ChannelMultiplexerTest, OneChannel) {
  std::unique_ptr<P2PStreamSocket> host_socket;
  std::unique_ptr<P2PStreamSocket> client_socket;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket, &client_socket));

  StreamConnectionTester tester(host_socket.get(), client_socket.get(),
                                kMessageSize, kMessages);
  base::RunLoop run_loop;
  tester.Start(run_loop.QuitClosure());
  run_loop.Run();
  tester.CheckResults();
}

TEST_F(ChannelMultiplexerTest, TwoChannels) {
  std::unique_ptr<P2PStreamSocket> host_socket1_;
  std::unique_ptr<P2PStreamSocket> client_socket1_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket1_, &client_socket1_));

  std::unique_ptr<P2PStreamSocket> host_socket2_;
  std::unique_ptr<P2PStreamSocket> client_socket2_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_socket2_, &client_socket2_));

  StreamConnectionTester tester1(host_socket1_.get(), client_socket1_.get(),
                                 kMessageSize, kMessages);
  StreamConnectionTester tester2(host_socket2_.get(), client_socket2_.get(),
                                 kMessageSize, kMessages);
  base::RunLoop run_loop;
  auto done_barrier = base::BarrierClosure(2, run_loop.QuitClosure());
  tester1.Start(done_barrier);
  tester2.Start(done_barrier);
  run_loop.Run();
  tester1.CheckResults();
  tester2.CheckResults();
}

// Four channels, two in each direction
TEST_F(ChannelMultiplexerTest, FourChannels) {
  std::unique_ptr<P2PStreamSocket> host_socket1_;
  std::unique_ptr<P2PStreamSocket> client_socket1_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket1_, &client_socket1_));

  std::unique_ptr<P2PStreamSocket> host_socket2_;
  std::unique_ptr<P2PStreamSocket> client_socket2_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_socket2_, &client_socket2_));

  std::unique_ptr<P2PStreamSocket> host_socket3;
  std::unique_ptr<P2PStreamSocket> client_socket3;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel("test3", &host_socket3, &client_socket3));

  std::unique_ptr<P2PStreamSocket> host_socket4;
  std::unique_ptr<P2PStreamSocket> client_socket4;
  ASSERT_NO_FATAL_FAILURE(CreateChannel("ch4", &host_socket4, &client_socket4));

  StreamConnectionTester tester1(host_socket1_.get(), client_socket1_.get(),
                                 kMessageSize, kMessages);
  StreamConnectionTester tester2(host_socket2_.get(), client_socket2_.get(),
                                 kMessageSize, kMessages);
  StreamConnectionTester tester3(client_socket3.get(), host_socket3.get(),
                                 kMessageSize, kMessages);
  StreamConnectionTester tester4(client_socket4.get(), host_socket4.get(),
                                 kMessageSize, kMessages);
  base::RunLoop run_loop;
  auto done_barrier = base::BarrierClosure(4, run_loop.QuitClosure());
  tester1.Start(done_barrier);
  tester2.Start(done_barrier);
  tester3.Start(done_barrier);
  tester4.Start(done_barrier);
  run_loop.Run();
  tester1.CheckResults();
  tester2.CheckResults();
  tester3.CheckResults();
  tester4.CheckResults();
}

TEST_F(ChannelMultiplexerTest, WriteFailSync) {
  std::unique_ptr<P2PStreamSocket> host_socket1_;
  std::unique_ptr<P2PStreamSocket> client_socket1_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket1_, &client_socket1_));

  std::unique_ptr<P2PStreamSocket> host_socket2_;
  std::unique_ptr<P2PStreamSocket> client_socket2_;
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_socket2_, &client_socket2_));

  FakeStreamSocket* socket =
      host_channel_factory_.GetFakeChannel(kMuxChannelName);
  socket->set_next_write_error(net::ERR_FAILED);
  socket->set_async_write(false);

  scoped_refptr<net::IOBufferWithSize> buf = CreateTestBuffer(100);

  base::MockCallback<net::CompletionRepeatingCallback> cb1, cb2;
  EXPECT_CALL(cb1, Run(net::ERR_FAILED));
  EXPECT_CALL(cb2, Run(net::ERR_FAILED));

  EXPECT_EQ(net::ERR_IO_PENDING,
            host_socket1_->Write(buf.get(), buf->size(), cb1.Get(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(net::ERR_IO_PENDING,
            host_socket2_->Write(buf.get(), buf->size(), cb2.Get(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ChannelMultiplexerTest, WriteFailAsync) {
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket1_, &client_socket1_));

  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_socket2_, &client_socket2_));

  FakeStreamSocket* socket =
      host_channel_factory_.GetFakeChannel(kMuxChannelName);
  socket->set_next_write_error(net::ERR_FAILED);
  socket->set_async_write(true);

  scoped_refptr<net::IOBufferWithSize> buf = CreateTestBuffer(100);

  base::MockCallback<net::CompletionRepeatingCallback> cb1, cb2;
  EXPECT_CALL(cb1, Run(net::ERR_FAILED));
  EXPECT_CALL(cb2, Run(net::ERR_FAILED));

  EXPECT_EQ(net::ERR_IO_PENDING,
            host_socket1_->Write(buf.get(), buf->size(), cb1.Get(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(net::ERR_IO_PENDING,
            host_socket2_->Write(buf.get(), buf->size(), cb2.Get(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ChannelMultiplexerTest, DeleteWhenFailed) {
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName, &host_socket1_, &client_socket1_));
  ASSERT_NO_FATAL_FAILURE(
      CreateChannel(kTestChannelName2, &host_socket2_, &client_socket2_));

  FakeStreamSocket* socket =
      host_channel_factory_.GetFakeChannel(kMuxChannelName);
  socket->set_next_write_error(net::ERR_FAILED);
  socket->set_async_write(true);

  scoped_refptr<net::IOBufferWithSize> buf = CreateTestBuffer(100);

  base::MockCallback<net::CompletionRepeatingCallback> cb1, cb2;
  EXPECT_CALL(cb1, Run(net::ERR_FAILED))
      .Times(AtMost(1))
      .WillOnce(InvokeWithoutArgs(this, &ChannelMultiplexerTest::DeleteAll));
  EXPECT_CALL(cb2, Run(net::ERR_FAILED))
      .Times(AtMost(1))
      .WillOnce(InvokeWithoutArgs(this, &ChannelMultiplexerTest::DeleteAll));

  EXPECT_EQ(net::ERR_IO_PENDING,
            host_socket1_->Write(buf.get(), buf->size(), cb1.Get(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(net::ERR_IO_PENDING,
            host_socket2_->Write(buf.get(), buf->size(), cb2.Get(),
                                 TRAFFIC_ANNOTATION_FOR_TESTS));

  base::RunLoop().RunUntilIdle();

  // Check that the sockets were destroyed.
  EXPECT_FALSE(host_mux_.get());
}

TEST_F(ChannelMultiplexerTest, SessionFail) {
  host_channel_factory_.set_asynchronous_create(true);
  host_channel_factory_.set_fail_create(true);

  MockConnectCallback cb1;
  MockConnectCallback cb2;

  host_mux_->CreateChannel(kTestChannelName,
                           base::BindOnce(&MockConnectCallback::OnConnected,
                                          base::Unretained(&cb1)));
  host_mux_->CreateChannel(kTestChannelName2,
                           base::BindOnce(&MockConnectCallback::OnConnected,
                                          base::Unretained(&cb2)));

  EXPECT_CALL(cb1, OnConnectedPtr(nullptr))
      .Times(AtMost(1))
      .WillOnce(InvokeWithoutArgs(
          this, &ChannelMultiplexerTest::DeleteAfterSessionFail));
  EXPECT_CALL(cb2, OnConnectedPtr(_)).Times(0);

  base::RunLoop().RunUntilIdle();
}

}  // namespace remoting::protocol
