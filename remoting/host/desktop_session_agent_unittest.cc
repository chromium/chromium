// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_resolution.h"
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
  bool session_started = false;
  FakeListener listener(base::BindLambdaForTesting([&]() {
    session_started = true;
    Shutdown();
    delegate.reset();
    proxy.reset();
  }));
  proxy = IPC::ChannelProxy::Create(
      agent_->Initialize(delegate->GetWeakPtr()).release(),
      IPC::Channel::MODE_CLIENT, &listener, task_runner_,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  mojo::AssociatedRemote<mojom::DesktopSessionAgent> desktop_session_agent;
  proxy->GetRemoteAssociatedInterface(&desktop_session_agent);
  // Let the IPC machinery finish up the interface request before using it.
  task_environment_.RunUntilIdle();

  bool remote_received = false;
  desktop_session_agent->Start(
      "jid", ScreenResolution(), DesktopEnvironmentOptions(),
      base::BindLambdaForTesting(
          [&](mojo::PendingAssociatedRemote<mojom::DesktopSessionControl>
                  pending_remote) {
            // Indicate that we received the desktop_session_control remote.
            remote_received = true;
            // Release any references to the other IPC classes.
            desktop_session_agent.reset();
          }));

  run_loop_.Run();

  ASSERT_TRUE(remote_received);
  ASSERT_TRUE(session_started);
}

}  // namespace remoting
