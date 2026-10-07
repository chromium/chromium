// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_renderer_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/base/mock_filters.h"
#include "media/remoting/mock_receiver_controller.h"
#include "media/remoting/receiver_controller.h"
#include "media/remoting/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"
#include "ui/gfx/color_space.h"

using openscreen::cast::RpcMessenger;
using testing::_;
using testing::NiceMock;
using testing::Return;

namespace media::remoting {

namespace {

class FakeRendererFactory : public RendererFactory {
 public:
  FakeRendererFactory() = default;
  ~FakeRendererFactory() override = default;

  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) override {
    return std::make_unique<NiceMock<MockRenderer>>();
  }
};

}  // namespace

class RemotingRendererFactoryTest : public testing::Test {
 public:
  RemotingRendererFactoryTest() = default;
  ~RemotingRendererFactoryTest() override = default;

  void SetUp() override {
    controller_ = ReceiverController::GetInstance();
    ResetForTesting(controller_);
    remotee_ = std::make_unique<MockRemotee>();
    remotee_->set_send_message_to_source_cb(
        base::BindRepeating(&RemotingRendererFactoryTest::OnSendMessageToSource,
                            base::Unretained(this)));
  }

  void TearDown() override {
    ResetForTesting(controller_);
    remotee_.reset();
  }

  void DrainMainThreadTasks() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void SendRpcAcquireRenderer(int sender_renderer_handle) {
    auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
    rpc->set_handle(RpcMessenger::kAcquireRendererHandle);
    rpc->set_proc(openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER);
    rpc->set_integer_value(sender_renderer_handle);
    controller_->rpc_messenger()->ProcessMessageFromRemote(std::move(rpc));
  }

  void OnSendMessageToSource(const std::vector<uint8_t>& message) {
    openscreen::cast::RpcMessage rpc;
    ASSERT_TRUE(rpc.ParseFromArray(message.data(), message.size()));
    if (rpc.proc() == openscreen::cast::RpcMessage::RPC_ACQUIRE_RENDERER_DONE) {
      acquire_renderer_done_count_++;
      last_receiver_rpc_handle_ = rpc.integer_value();
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<ReceiverController> controller_ = nullptr;
  std::unique_ptr<MockRemotee> remotee_;
  int acquire_renderer_done_count_ = 0;
  int last_receiver_rpc_handle_ = RpcMessenger::kInvalidHandle;
};

TEST_F(RemotingRendererFactoryTest, CreateRendererAndAcquireRendererOnThreads) {
  base::Thread media_thread("TestMediaThread");
  ASSERT_TRUE(media_thread.Start());

  auto factory = std::make_unique<RemotingRendererFactory>(
      remotee_->BindNewPipeAndPassRemote(),
      std::make_unique<FakeRendererFactory>(), media_thread.task_runner());

  std::unique_ptr<Renderer> renderer =
      factory->CreateRenderer(media_thread.task_runner(), nullptr, nullptr,
                              nullptr, base::NullCallback(), gfx::ColorSpace());
  ASSERT_TRUE(renderer);

  // Send RPC_ACQUIRE_RENDERER on the main thread. This sets the remote handle
  // on the main thread and posts Receiver::SetRemoteHandle to `media_thread`.
  constexpr int kSenderRendererHandle = 42;
  SendRpcAcquireRenderer(kSenderRendererHandle);

  // Flush `media_thread` so Receiver::SetRemoteHandle runs and posts
  // OnAcquireRendererDone back to the main thread.
  media_thread.FlushForTesting();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return acquire_renderer_done_count_ == 1; }));

  EXPECT_EQ(1, acquire_renderer_done_count_);
  EXPECT_NE(RpcMessenger::kInvalidHandle, last_receiver_rpc_handle_);

  // Destroy the created Renderer on the media sequence.
  media_thread.task_runner()->DeleteSoon(FROM_HERE, std::move(renderer));
  media_thread.FlushForTesting();
  DrainMainThreadTasks();
}

// Regression test for crbug.com/500600456: destroying RemotingRendererFactory
// on the main sequence while Receiver::SetRemoteHandle / OnAcquireRendererDone
// is in flight across sequences must not cause a cross-sequence WeakPtr race or
// UAF.
TEST_F(RemotingRendererFactoryTest,
       DestroyFactoryWhileAcquireRendererDoneInFlight) {
  base::Thread media_thread("TestMediaThread");
  ASSERT_TRUE(media_thread.Start());

  auto factory = std::make_unique<RemotingRendererFactory>(
      remotee_->BindNewPipeAndPassRemote(),
      std::make_unique<FakeRendererFactory>(), media_thread.task_runner());

  std::unique_ptr<Renderer> renderer =
      factory->CreateRenderer(media_thread.task_runner(), nullptr, nullptr,
                              nullptr, base::NullCallback(), gfx::ColorSpace());
  ASSERT_TRUE(renderer);

  // Queue Receiver::SetRemoteHandle onto `media_thread`.
  constexpr int kSenderRendererHandle = 99;
  SendRpcAcquireRenderer(kSenderRendererHandle);

  // Destroy `factory` on the main sequence before `media_thread` runs
  // Receiver::SetRemoteHandle or posts OnAcquireRendererDone back.
  factory.reset();

  // Flushing `media_thread` and draining main sequence tasks must safely drop
  // the invalidated WeakPtr callback without crashing or UAF.
  media_thread.FlushForTesting();
  DrainMainThreadTasks();

  EXPECT_EQ(0, acquire_renderer_done_count_);

  media_thread.task_runner()->DeleteSoon(FROM_HERE, std::move(renderer));
  media_thread.FlushForTesting();
  DrainMainThreadTasks();
}

}  // namespace media::remoting
