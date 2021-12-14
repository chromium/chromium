// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/fake_desktop_environment.h"
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

  void CrashNetworkProcess(const base::Location& location) override {}

  base::WeakPtr<Delegate> GetWeakPtr() { return weak_ptr_.GetWeakPtr(); }

 private:
  FakeDesktopEnvironmentFactory factory_;

  base::WeakPtrFactory<FakeDelegate> weak_ptr_{this};
};

FakeDelegate::FakeDelegate(scoped_refptr<base::SingleThreadTaskRunner> runner)
    : factory_(runner) {}

class FakeListener : public IPC::Listener {
 public:
  explicit FakeListener(base::RepeatingClosure action_after_received)
      : action_after_received_(action_after_received) {}
  ~FakeListener() override = default;

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  const base::RepeatingClosure action_after_received_;
};

bool FakeListener::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void FakeListener::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  EXPECT_EQ(mojom::DesktopSessionEventHandler::Name_, interface_name);
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

TEST_F(DesktopSessionAgentTest, StartDesktopSessionAgent) {
  std::unique_ptr<FakeDelegate> delegate(new FakeDelegate(task_runner_));
  std::unique_ptr<IPC::ChannelProxy> proxy;
  FakeListener listener(base::BindRepeating(
      [](DesktopSessionAgentTest* test, std::unique_ptr<FakeDelegate>* delegate,
         std::unique_ptr<IPC::ChannelProxy>* proxy) {
        test->Shutdown();
        delegate->reset();
        proxy->reset();
      },
      base::Unretained(this), base::Unretained(&delegate),
      base::Unretained(&proxy)));
  proxy = IPC::ChannelProxy::Create(
      agent_->Start(delegate->GetWeakPtr()).release(),
      IPC::Channel::MODE_CLIENT, &listener, task_runner_,
      base::ThreadTaskRunnerHandle::Get());
  ASSERT_TRUE(proxy->Send(new ChromotingNetworkDesktopMsg_StartSessionAgent(
      "jid", ScreenResolution(), DesktopEnvironmentOptions())));
  run_loop_.Run();
}

}  // namespace remoting
