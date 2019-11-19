// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_video_dispatcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "remoting/base/buffered_socket_writer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/fake_stream_socket.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/stream_message_pipe_adapter.h"
#include "remoting/protocol/video_stub.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

class ClientVideoDispatcherTest : public testing::Test,
                                  public VideoStub,
                                  public ChannelDispatcherBase::EventHandler {
 public:
  ClientVideoDispatcherTest();

  // VideoStub interface.
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> video_packet,
                          base::OnceClosure done) override;

  // ChannelDispatcherBase::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;
  void OnChannelClosed(ChannelDispatcherBase* channel_dispatcher) override;

 protected:
  void OnChannelError(int error);

  void OnMessageReceived(std::unique_ptr<CompoundBuffer> buffer);
  void OnReadError(int error);

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Set to true in OnChannelInitialized().
  bool initialized_ = false;

  // Client side.
  FakeStreamChannelFactory client_channel_factory_;
  StreamMessageChannelFactoryAdapter channel_factory_adapter_;
  MockClientStub client_stub_;
  ClientVideoDispatcher dispatcher_;

  // Host side.
  FakeStreamSocket host_socket_;
  MessageReader reader_;
  BufferedSocketWriter writer_;

  std::vector<std::unique_ptr<VideoPacket>> video_packets_;
  std::vector<base::OnceClosure> packet_done_callbacks_;

  std::vector<std::unique_ptr<VideoAck>> ack_messages_;
};

ClientVideoDispatcherTest::ClientVideoDispatcherTest()
    : channel_factory_adapter_(
          &client_channel_factory_,
          base::Bind(&ClientVideoDispatcherTest::OnChannelError,
                     base::Unretained(this))),
      dispatcher_(this, &client_stub_) {
  dispatcher_.Init(&channel_factory_adapter_, this);
  base::RunLoop().RunUntilIdle();
  DCHECK(initialized_);
  host_socket_.PairWith(
      client_channel_factory_.GetFakeChannel(kVideoChannelName));
  reader_.StartReading(&host_socket_,
                       base::Bind(&ClientVideoDispatcherTest::OnMessageReceived,
                                  base::Unretained(this)),
                       base::Bind(&ClientVideoDispatcherTest::OnReadError,
                                  base::Unretained(this)));
  writer_.Start(
      base::Bind(&P2PStreamSocket::Write, base::Unretained(&host_socket_)),
      BufferedSocketWriter::WriteFailedCallback());
}

void ClientVideoDispatcherTest::ProcessVideoPacket(
    std::unique_ptr<VideoPacket> video_packet,
    base::OnceClosure done) {
  video_packets_.push_back(std::move(video_packet));
  packet_done_callbacks_.push_back(std::move(done));
}

void ClientVideoDispatcherTest::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  initialized_ = true;
}

void ClientVideoDispatcherTest::OnChannelClosed(
    ChannelDispatcherBase* channel_dispatcher) {
  // Don't expect channels to be closed.
  FAIL();
}

void ClientVideoDispatcherTest::OnChannelError(int error) {
  // Don't expect channel creation to fail.
  FAIL();
}

void ClientVideoDispatcherTest::OnMessageReceived(
    std::unique_ptr<CompoundBuffer> buffer) {
  std::unique_ptr<VideoAck> ack = ParseMessage<VideoAck>(buffer.get());
  EXPECT_TRUE(ack);
  ack_messages_.push_back(std::move(ack));
}

void ClientVideoDispatcherTest::OnReadError(int error) {
  LOG(FATAL) << "Unexpected read error: " << error;
}

// Verify that the client can receive video packets and acks are not sent for
// VideoPackets that don't have frame_id field set.
TEST_F(ClientVideoDispatcherTest, WithoutAcks) {
  VideoPacket packet;
  packet.set_data(std::string());

  // Send a VideoPacket and verify that the client receives it.
  writer_.Write(SerializeAndFrameMessage(packet), base::Closure(),
                TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, video_packets_.size());

  std::move(packet_done_callbacks_.front()).Run();
  base::RunLoop().RunUntilIdle();

  // Ack should never be sent for the packet without frame_id.
  EXPECT_TRUE(ack_messages_.empty());
}

// Verifies that the dispatcher sends Ack message with correct rendering delay.
TEST_F(ClientVideoDispatcherTest, WithAcks) {
  int kTestFrameId = 3;

  VideoPacket packet;
  packet.set_data(std::string());
  packet.set_frame_id(kTestFrameId);

  // Send a VideoPacket and verify that the client receives it.
  writer_.Write(SerializeAndFrameMessage(packet), base::Closure(),
                TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, video_packets_.size());

  // Ack should only be sent after the packet is processed.
  EXPECT_TRUE(ack_messages_.empty());
  base::RunLoop().RunUntilIdle();

  // Fake completion of video packet decoding, to trigger the Ack.
  std::move(packet_done_callbacks_.front()).Run();
  base::RunLoop().RunUntilIdle();

  // Verify that the Ack message has been received.
  ASSERT_EQ(1U, ack_messages_.size());
  EXPECT_EQ(kTestFrameId, ack_messages_[0]->frame_id());
}

// Verifies that the dispatcher properly synthesizes VideoLayout message when
// screen size changes.
TEST_F(ClientVideoDispatcherTest, VideoLayout) {
  const int kScreenSize = 100;
  const float kScaleFactor = 2.0;

  VideoPacket packet;
  packet.set_data(std::string());
  packet.set_frame_id(42);
  packet.mutable_format()->set_screen_width(kScreenSize);
  packet.mutable_format()->set_screen_height(kScreenSize);
  packet.mutable_format()->set_x_dpi(kDefaultDpi * kScaleFactor);
  packet.mutable_format()->set_y_dpi(kDefaultDpi * kScaleFactor);

  VideoLayout layout;
  EXPECT_CALL(client_stub_, SetVideoLayout(testing::_))
      .WillOnce(testing::SaveArg<0>(&layout));

  // Send a VideoPacket and verify that the client receives it.
  writer_.Write(SerializeAndFrameMessage(packet), base::Closure(),
                TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1, layout.video_track_size());
  EXPECT_EQ(0, layout.video_track(0).position_x());
  EXPECT_EQ(0, layout.video_track(0).position_y());
  EXPECT_EQ(kScreenSize / kScaleFactor, layout.video_track(0).width());
  EXPECT_EQ(kScreenSize / kScaleFactor, layout.video_track(0).height());
  EXPECT_EQ(kDefaultDpi * kScaleFactor, layout.video_track(0).x_dpi());
  EXPECT_EQ(kDefaultDpi * kScaleFactor, layout.video_track(0).y_dpi());
}

// Verify that Ack messages are sent in correct order.
TEST_F(ClientVideoDispatcherTest, AcksOrder) {
  int kTestFrameId = 3;

  VideoPacket packet;
  packet.set_data(std::string());
  packet.set_frame_id(kTestFrameId);

  // Send two VideoPackets.
  writer_.Write(SerializeAndFrameMessage(packet), base::Closure(),
                TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop().RunUntilIdle();

  packet.set_frame_id(kTestFrameId + 1);
  writer_.Write(SerializeAndFrameMessage(packet), base::Closure(),
                TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2U, video_packets_.size());
  EXPECT_TRUE(ack_messages_.empty());

  // Call completion callbacks in revers order.
  std::move(packet_done_callbacks_[1]).Run();
  std::move(packet_done_callbacks_[0]).Run();

  base::RunLoop().RunUntilIdle();

  // Verify order of Ack messages.
  ASSERT_EQ(2U, ack_messages_.size());
  EXPECT_EQ(kTestFrameId, ack_messages_[0]->frame_id());
  EXPECT_EQ(kTestFrameId + 1, ack_messages_[1]->frame_id());
}

}  // namespace protocol
}  // namespace remoting
