// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>
#include <tuple>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/perf_log.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_perftest_messages.h"
#include "ipc/ipc_perftest_util.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_test.mojom.h"
#include "ipc/ipc_test_base.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/core/test/multiprocess_test_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {
namespace {

struct TestParams {
  TestParams() = default;
  TestParams(size_t in_message_size,
             size_t in_frames_per_second,
             size_t in_messages_per_frame,
             size_t in_duration_in_seconds)
      : message_size(in_message_size),
        frames_per_second(in_frames_per_second),
        messages_per_frame(in_messages_per_frame),
        duration_in_seconds(in_duration_in_seconds) {}

  size_t message_size;
  size_t frames_per_second;
  size_t messages_per_frame;
  size_t duration_in_seconds;
};

std::vector<TestParams> GetDefaultTestParams() {
  std::vector<TestParams> list;
  list.push_back({144, 20, 10, 10});
  list.push_back({144, 60, 10, 10});
  return list;
}

std::string GetLogTitle(const std::string& label, const TestParams& params) {
  return base::StringPrintf(
      "%s_MsgSize_%zu_FrmPerSec_%zu_MsgPerFrm_%zu", label.c_str(),
      params.message_size, params.frames_per_second, params.messages_per_frame);
}

base::TimeDelta GetFrameTime(size_t frames_per_second) {
  return base::Seconds(1.0 / frames_per_second);
}

class PerfCpuLogger {
 public:
  explicit PerfCpuLogger(std::string_view test_name)
      : test_name_(test_name),
        process_metrics_(base::ProcessMetrics::CreateCurrentProcessMetrics()) {
    // Query the CPU usage once to start the recording interval.
    const double inital_cpu_usage =
        process_metrics_->GetPlatformIndependentCPUUsage().value_or(-1.0);
    // This should have been the first call so the reported cpu usage should be
    // exactly zero.
    DCHECK_EQ(inital_cpu_usage, 0.0);
  }

  PerfCpuLogger(const PerfCpuLogger&) = delete;
  PerfCpuLogger& operator=(const PerfCpuLogger&) = delete;

  ~PerfCpuLogger() {
    const double result =
        process_metrics_->GetPlatformIndependentCPUUsage().value_or(-1.0);
    base::LogPerfResult(test_name_.c_str(), result, "%");
  }

 private:
  std::string test_name_;
  std::unique_ptr<base::ProcessMetrics> process_metrics_;
};

MULTIPROCESS_TEST_MAIN(MojoPerfTestClientTestChildMain) {
  MojoPerfTestClient client;
  int rv = mojo::core::test::MultiprocessTestHelper::RunClientMain(
      base::BindOnce(&MojoPerfTestClient::Run, base::Unretained(&client)),
      true /* pass_pipe_ownership_to_main */);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  return rv;
}

class ChannelSteadyPingPongListener : public Listener {
 public:
  ChannelSteadyPingPongListener() = default;

  ~ChannelSteadyPingPongListener() override = default;

  void Init(Sender* sender) {
    DCHECK(!sender_);
    sender_ = sender;
  }

  void SetTestParams(const TestParams& params,
                     const std::string& label,
                     bool sync,
                     base::OnceClosure quit_closure) {
    params_ = params;
    label_ = label;
    sync_ = sync;
    quit_closure_ = std::move(quit_closure);
    payload_ = std::string(params.message_size, 'a');
  }

  bool OnMessageReceived(const Message& message) override {
    CHECK(sender_);

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(ChannelSteadyPingPongListener, message)
      IPC_MESSAGE_HANDLER(TestMsg_Hello, OnHello)
      IPC_MESSAGE_HANDLER(TestMsg_Ping, OnPing)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnHello() {
    cpu_logger_ = std::make_unique<PerfCpuLogger>(GetLogTitle(label_, params_));

    frame_count_down_ = params_.frames_per_second * params_.duration_in_seconds;

    timer_.Start(FROM_HERE, GetFrameTime(params_.frames_per_second), this,
                 &ChannelSteadyPingPongListener::StartPingPong);
  }

  void StartPingPong() {
    if (sync_) {
      base::TimeTicks before = base::TimeTicks::Now();
      for (count_down_ = params_.messages_per_frame; count_down_ > 0;
           --count_down_) {
        std::string response;
        sender_->Send(new TestMsg_SyncPing(payload_, &response));
        DCHECK_EQ(response, payload_);
      }

      if (base::TimeTicks::Now() - before >
          GetFrameTime(params_.frames_per_second)) {
        LOG(ERROR) << "Frame " << frame_count_down_
                   << " wasn't able to complete on time!";
      }

      CHECK_GT(frame_count_down_, 0);
      frame_count_down_--;
      if (frame_count_down_ == 0)
        StopPingPong();
    } else {
      if (count_down_ != 0) {
        LOG(ERROR) << "Frame " << frame_count_down_
                   << " wasn't able to complete on time!";
      } else {
        SendPong();
      }
      count_down_ = params_.messages_per_frame;
    }
  }

  void StopPingPong() {
    cpu_logger_.reset();
    timer_.AbandonAndStop();
    std::move(quit_closure_).Run();
  }

  void OnPing(const std::string& payload) {
    // Include message deserialization in latency.
    DCHECK_EQ(payload_.size(), payload.size());

    CHECK_GT(count_down_, 0);
    count_down_--;
    if (count_down_ > 0) {
      SendPong();
    } else {
      CHECK_GT(frame_count_down_, 0);
      frame_count_down_--;
      if (frame_count_down_ == 0)
        StopPingPong();
    }
  }

  void SendPong() { sender_->Send(new TestMsg_Ping(payload_)); }

 private:
  raw_ptr<Sender> sender_ = nullptr;
  TestParams params_;
  std::string payload_;
  std::string label_;
  bool sync_ = false;

  int count_down_ = 0;
  int frame_count_down_ = 0;

  base::RepeatingTimer timer_;
  std::unique_ptr<PerfCpuLogger> cpu_logger_;

  base::OnceClosure quit_closure_;
};

class ChannelSteadyPingPongTest : public IPCChannelMojoTestBase {
 public:
  ChannelSteadyPingPongTest() = default;
  ~ChannelSteadyPingPongTest() override = default;

  void RunPingPongServer(const std::string& label, bool sync) {
    Init("MojoPerfTestClient");

    // Set up IPC channel and start client.
    ChannelSteadyPingPongListener listener;

    std::unique_ptr<ChannelProxy> channel_proxy;
    std::unique_ptr<base::WaitableEvent> shutdown_event;

    if (sync) {
      shutdown_event = std::make_unique<base::WaitableEvent>(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED);
      channel_proxy = IPC::SyncChannel::Create(
          TakeHandle().release(), IPC::Channel::MODE_SERVER, &listener,
          GetIOThreadTaskRunner(),
          base::SingleThreadTaskRunner::GetCurrentDefault(), false,
          shutdown_event.get());
    } else {
      channel_proxy = IPC::ChannelProxy::Create(
          TakeHandle().release(), IPC::Channel::MODE_SERVER, &listener,
          GetIOThreadTaskRunner(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
    }
    listener.Init(channel_proxy.get());

    LockThreadAffinity thread_locker(kSharedCore);
    std::vector<TestParams> params_list = GetDefaultTestParams();
    for (const auto& params : params_list) {
      base::RunLoop run_loop;

      listener.SetTestParams(params, label, sync,
                             run_loop.QuitWhenIdleClosure());

      // This initial message will kick-start the ping-pong of messages.
      channel_proxy->Send(new TestMsg_Hello);

      run_loop.Run();
    }

    // Send quit message.
    channel_proxy->Send(new TestMsg_Quit);

    EXPECT_TRUE(WaitForClientShutdown());
    channel_proxy.reset();
  }
};

TEST_F(ChannelSteadyPingPongTest, AsyncPingPong) {
  RunPingPongServer("IPC_CPU_Async", false);
}

TEST_F(ChannelSteadyPingPongTest, SyncPingPong) {
  RunPingPongServer("IPC_CPU_Sync", true);
}

class MojoSteadyPingPongTest : public mojo::core::test::MojoTestBase {
 public:
  MojoSteadyPingPongTest() = default;

  MojoSteadyPingPongTest(const MojoSteadyPingPongTest&) = delete;
  MojoSteadyPingPongTest& operator=(const MojoSteadyPingPongTest&) = delete;

 protected:
  void RunPingPongServer(MojoHandle mp, const std::string& label, bool sync) {
    label_ = label;
    sync_ = sync;

    mojo::MessagePipeHandle mp_handle(mp);
    mojo::ScopedMessagePipeHandle scoped_mp(mp_handle);
    ping_receiver_.Bind(
        mojo::PendingRemote<IPC::mojom::Reflector>(std::move(scoped_mp), 0u));

    LockThreadAffinity thread_locker(kSharedCore);
    std::vector<TestParams> params_list = GetDefaultTestParams();
    for (const auto& params : params_list) {
      params_ = params;
      payload_ = std::string(params.message_size, 'a');

      ping_receiver_->Ping("hello",
                           base::BindOnce(&MojoSteadyPingPongTest::OnHello,
                                          base::Unretained(this)));
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitWhenIdleClosure();
      run_loop.Run();
    }

    ping_receiver_->Quit();

    std::ignore = ping_receiver_.Unbind().PassPipe().release();
  }

  void OnHello(const std::string& value) {
    cpu_logger_ = std::make_unique<PerfCpuLogger>(GetLogTitle(label_, params_));

    frame_count_down_ = params_.frames_per_second * params_.duration_in_seconds;

    timer_.Start(FROM_HERE, GetFrameTime(params_.frames_per_second), this,
                 &MojoSteadyPingPongTest::StartPingPong);
  }

  void StartPingPong() {
    if (sync_) {
      base::TimeTicks before = base::TimeTicks::Now();
      for (count_down_ = params_.messages_per_frame; count_down_ > 0;
           --count_down_) {
        std::string response;
        ping_receiver_->SyncPing(payload_, &response);
        DCHECK_EQ(response, payload_);
      }

      if (base::TimeTicks::Now() - before >
          GetFrameTime(params_.frames_per_second)) {
        LOG(ERROR) << "Frame " << frame_count_down_
                   << " wasn't able to complete on time!";
      }

      CHECK_GT(frame_count_down_, 0);
      frame_count_down_--;
      if (frame_count_down_ == 0)
        StopPingPong();
    } else {
      if (count_down_ != 0) {
        LOG(ERROR) << "Frame " << frame_count_down_
                   << " wasn't able to complete on time!";
      } else {
        SendPing();
      }
      count_down_ = params_.messages_per_frame;
    }
  }

  void StopPingPong() {
    cpu_logger_.reset();
    timer_.AbandonAndStop();
    std::move(quit_closure_).Run();
  }

  void OnPong(const std::string& value) {
    // Include message deserialization in latency.
    DCHECK_EQ(payload_.size(), value.size());

    CHECK_GT(count_down_, 0);
    count_down_--;
    if (count_down_ > 0) {
      SendPing();
    } else {
      CHECK_GT(frame_count_down_, 0);
      frame_count_down_--;
      if (frame_count_down_ == 0)
        StopPingPong();
    }
  }

  void SendPing() {
    ping_receiver_->Ping(payload_,
                         base::BindOnce(&MojoSteadyPingPongTest::OnPong,
                                        base::Unretained(this)));
  }

  static int RunPingPongClient(MojoHandle mp) {
    mojo::MessagePipeHandle mp_handle(mp);
    mojo::ScopedMessagePipeHandle scoped_mp(mp_handle);

    LockThreadAffinity thread_locker(kSharedCore);
    base::RunLoop run_loop;
    ReflectorImpl impl(std::move(scoped_mp), run_loop.QuitWhenIdleClosure());
    run_loop.Run();
    return 0;
  }

 private:
  TestParams params_;
  std::string payload_;
  std::string label_;
  bool sync_ = false;

  mojo::Remote<IPC::mojom::Reflector> ping_receiver_;

  int count_down_ = 0;
  int frame_count_down_ = 0;

  base::RepeatingTimer timer_;
  std::unique_ptr<PerfCpuLogger> cpu_logger_;

  base::OnceClosure quit_closure_;
};

DEFINE_TEST_CLIENT_WITH_PIPE(PingPongClient, MojoSteadyPingPongTest, h) {
  base::test::SingleThreadTaskEnvironment task_environment;
  return RunPingPongClient(h);
}

// Similar to ChannelSteadyPingPongTest above, but uses a Mojo interface
// instead of raw IPC::Messages.
TEST_F(MojoSteadyPingPongTest, AsyncPingPong) {
  RunTestClient("PingPongClient", [&](MojoHandle h) {
    base::test::SingleThreadTaskEnvironment task_environment;
    RunPingPongServer(h, "Mojo_CPU_Async", false);
  });
}

TEST_F(MojoSteadyPingPongTest, SyncPingPong) {
  RunTestClient("PingPongClient", [&](MojoHandle h) {
    base::test::SingleThreadTaskEnvironment task_environment;
    RunPingPongServer(h, "Mojo_CPU_Sync", true);
  });
}

}  // namespace
}  // namespace IPC
