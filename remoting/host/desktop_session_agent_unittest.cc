// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_environment_options.h"
#include "remoting/host/fake_desktop_environment.h"
#include "remoting/host/screen_resolution.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class FakeDelegate : public DesktopSessionAgent::Delegate {
 public:
  FakeDelegate(scoped_refptr<base::SingleThreadTaskRunner> runner);
  ~FakeDelegate() override = default;

  DesktopEnvironmentFactory& desktop_environment_factory() override {
    return factory_;
  }

  void OnNetworkProcessDisconnected() override {}

  base::WeakPtr<Delegate> GetWeakPtr() { return weak_ptr_.GetWeakPtr(); }

 private:
  FakeDesktopEnvironmentFactory factory_;

  base::WeakPtrFactory<FakeDelegate> weak_ptr_{this};
};

FakeDelegate::FakeDelegate(scoped_refptr<base::SingleThreadTaskRunner> runner)
    : factory_(runner) {}

class ProcessStatsListener : public IPC::Listener {
 public:
  ProcessStatsListener(base::Closure action_after_received)
      : action_after_received_(action_after_received) {}

  ~ProcessStatsListener() override = default;

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnProcessResourceUsage(
      const remoting::protocol::AggregatedProcessResourceUsage& usage);

  const base::Closure action_after_received_;
};

bool ProcessStatsListener::OnMessageReceived(const IPC::Message& message) {
  bool handled = false;
  IPC_BEGIN_MESSAGE_MAP(ProcessStatsListener, message)
    IPC_MESSAGE_HANDLER(ChromotingAnyToNetworkMsg_ReportProcessStats,
                        OnProcessResourceUsage);
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ProcessStatsListener::OnProcessResourceUsage(
    const remoting::protocol::AggregatedProcessResourceUsage& usage) {
  action_after_received_.Run();
}

}  // namespace

class DesktopSessionAgentTest : public ::testing::Test {
 public:
  DesktopSessionAgentTest();
  ~DesktopSessionAgentTest() override = default;

  void Shutdown();

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;
  scoped_refptr<DesktopSessionAgent> agent_;
};

DesktopSessionAgentTest::DesktopSessionAgentTest()
    : task_runner_(
          new AutoThreadTaskRunner(task_environment_.GetMainThreadTaskRunner(),
                                   run_loop_.QuitClosure())),
      agent_(new DesktopSessionAgent(task_runner_,
                                     task_runner_,
                                     task_runner_,
                                     task_runner_)) {}

void DesktopSessionAgentTest::Shutdown() {
  task_runner_ = nullptr;
  agent_->Stop();
  agent_ = nullptr;
}

TEST_F(DesktopSessionAgentTest, StartProcessStatsReport) {
  std::unique_ptr<FakeDelegate> delegate(new FakeDelegate(task_runner_));
  std::unique_ptr<IPC::ChannelProxy> proxy;
  ProcessStatsListener listener(base::Bind([](
          DesktopSessionAgentTest* test,
          std::unique_ptr<FakeDelegate>* delegate,
          std::unique_ptr<IPC::ChannelProxy>* proxy) {
        test->Shutdown();
        delegate->reset();
        proxy->reset();
      },
      base::Unretained(this),
      base::Unretained(&delegate),
      base::Unretained(&proxy)));
  proxy = IPC::ChannelProxy::Create(
      agent_->Start(delegate->GetWeakPtr()).release(),
      IPC::Channel::MODE_CLIENT, &listener, task_runner_,
      base::ThreadTaskRunnerHandle::Get());
  ASSERT_TRUE(proxy->Send(new ChromotingNetworkDesktopMsg_StartSessionAgent(
      "jid", ScreenResolution(), DesktopEnvironmentOptions())));
  ASSERT_TRUE(proxy->Send(new ChromotingNetworkToAnyMsg_StartProcessStatsReport(
      base::TimeDelta::FromMilliseconds(1))));
  run_loop_.Run();
}

TEST_F(DesktopSessionAgentTest, StartProcessStatsReportWithInvalidInterval) {
  std::unique_ptr<FakeDelegate> delegate(new FakeDelegate(task_runner_));
  std::unique_ptr<IPC::ChannelProxy> proxy;
  ProcessStatsListener listener{base::DoNothing()};
  proxy = IPC::ChannelProxy::Create(
      agent_->Start(delegate->GetWeakPtr()).release(),
      IPC::Channel::MODE_CLIENT, &listener, task_runner_,
      base::ThreadTaskRunnerHandle::Get());
  ASSERT_TRUE(proxy->Send(new ChromotingNetworkDesktopMsg_StartSessionAgent(
      "jid", ScreenResolution(), DesktopEnvironmentOptions())));
  ASSERT_TRUE(proxy->Send(new ChromotingNetworkToAnyMsg_StartProcessStatsReport(
      base::TimeDelta::FromMilliseconds(-1))));
  ASSERT_TRUE(proxy->Send(
      new ChromotingNetworkToAnyMsg_StopProcessStatsReport()));
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](DesktopSessionAgentTest* test,
             std::unique_ptr<FakeDelegate>* delegate,
             std::unique_ptr<IPC::ChannelProxy>* proxy) {
            test->Shutdown();
            delegate->reset();
            proxy->reset();
          },
          base::Unretained(this), base::Unretained(&delegate),
          base::Unretained(&proxy)),
      base::TimeDelta::FromMilliseconds(1));
  run_loop_.Run();
}

TEST_F(DesktopSessionAgentTest, StartThenStopProcessStatsReport) {
  std::unique_ptr<FakeDelegate> delegate(new FakeDelegate(task_runner_));
  std::unique_ptr<IPC::ChannelProxy> proxy;
  ProcessStatsListener listener{base::DoNothing()};
  proxy = IPC::ChannelProxy::Create(
      agent_->Start(delegate->GetWeakPtr()).release(),
      IPC::Channel::MODE_CLIENT, &listener, task_runner_,
      base::ThreadTaskRunnerHandle::Get());
  ASSERT_TRUE(proxy->Send(new ChromotingNetworkDesktopMsg_StartSessionAgent(
      "jid", ScreenResolution(), DesktopEnvironmentOptions())));
  ASSERT_TRUE(proxy->Send(new ChromotingNetworkToAnyMsg_StartProcessStatsReport(
      base::TimeDelta::FromMilliseconds(1))));
  ASSERT_TRUE(proxy->Send(
      new ChromotingNetworkToAnyMsg_StopProcessStatsReport()));
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](DesktopSessionAgentTest* test,
             std::unique_ptr<FakeDelegate>* delegate,
             std::unique_ptr<IPC::ChannelProxy>* proxy) {
            test->Shutdown();
            delegate->reset();
            proxy->reset();
          },
          base::Unretained(this), base::Unretained(&delegate),
          base::Unretained(&proxy)),
      base::TimeDelta::FromMilliseconds(1));
  run_loop_.Run();
}

TEST_F(DesktopSessionAgentTest, SendAggregatedProcessResourceUsage) {
  std::unique_ptr<IPC::Channel> receiver;
  std::unique_ptr<IPC::Channel> sender;
  ProcessStatsListener listener(base::Bind([](
          DesktopSessionAgentTest* test,
          std::unique_ptr<IPC::Channel>* receiver,
          std::unique_ptr<IPC::Channel>* sender) {
        test->Shutdown();
        base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
            FROM_HERE, receiver->release());
        base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
            FROM_HERE, sender->release());
      },
      base::Unretained(this),
      base::Unretained(&receiver),
      base::Unretained(&sender)));
  mojo::MessagePipe pipe;
  receiver = IPC::Channel::CreateServer(
      pipe.handle1.release(),
      &listener,
      task_runner_);
  ASSERT_TRUE(receiver->Connect());
  sender = IPC::Channel::CreateClient(
      pipe.handle0.release(),
      &listener,
      task_runner_);
  ASSERT_TRUE(sender->Connect());
  protocol::AggregatedProcessResourceUsage aggregated;
  for (int i = 0; i < 2; i++) {
    *aggregated.add_usages() = protocol::ProcessResourceUsage();
  }
  ASSERT_TRUE(sender->Send(
      new ChromotingAnyToNetworkMsg_ReportProcessStats(aggregated)));
  run_loop_.Run();
}

TEST_F(DesktopSessionAgentTest, SendEmptyAggregatedProcessResourceUsage) {
  std::unique_ptr<IPC::Channel> receiver;
  std::unique_ptr<IPC::Channel> sender;
  ProcessStatsListener listener(base::Bind([](
          DesktopSessionAgentTest* test,
          std::unique_ptr<IPC::Channel>* receiver,
          std::unique_ptr<IPC::Channel>* sender) {
        test->Shutdown();
        base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
            FROM_HERE, receiver->release());
        base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
            FROM_HERE, sender->release());
      },
      base::Unretained(this),
      base::Unretained(&receiver),
      base::Unretained(&sender)));
  mojo::MessagePipe pipe;
  receiver = IPC::Channel::CreateServer(
      pipe.handle1.release(),
      &listener,
      task_runner_);
  ASSERT_TRUE(receiver->Connect());
  sender = IPC::Channel::CreateClient(
      pipe.handle0.release(),
      &listener,
      task_runner_);
  ASSERT_TRUE(sender->Connect());
  ASSERT_TRUE(sender->Send(new ChromotingAnyToNetworkMsg_ReportProcessStats(
      protocol::AggregatedProcessResourceUsage())));
  run_loop_.Run();
}

}  // namespace remoting
