// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/it2me_standalone_host.h"

#include <functional>
#include <iostream>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_extension.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/session_config.h"

namespace remoting {
namespace test {

namespace {

void OutputFakeConnectionEventLogger(const FakeConnectionEventLogger& logger) {
  std::cout << logger;
}

constexpr char kSessionJid[] = "user@domain/rest-of-jid";

}  // namespace

using ::remoting::protocol::MockSession;

It2MeStandaloneHost::It2MeStandaloneHost()
    : task_environment_(
          base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
      context_(ChromotingHostContext::Create(
          new AutoThreadTaskRunner(base::ThreadTaskRunnerHandle::Get(),
                                   run_loop_.QuitClosure()))),
      main_task_runner_(context_->file_task_runner()),
      factory_(main_task_runner_,
               context_->video_capture_task_runner(),
               context_->input_task_runner(),
               context_->ui_task_runner()),
      connection_(base::WrapUnique(new testing::NiceMock<MockSession>())),
      session_jid_(kSessionJid),
#if defined(OS_LINUX)
      // We cannot support audio capturing for linux, since a pipe name is
      // needed to initialize AudioCapturerLinux.
      config_(protocol::SessionConfig::ForTest()),
#else
      config_(protocol::SessionConfig::ForTestWithAudio()),
#endif
      event_logger_(&connection_) {
  EXPECT_CALL(*static_cast<MockSession*>(connection_.session()), jid())
      .WillRepeatedly(testing::ReturnRef(session_jid_));
  EXPECT_CALL(*static_cast<MockSession*>(connection_.session()), config())
      .WillRepeatedly(testing::ReturnRef(*config_));
  connection_.set_video_stub(event_logger_.video_stub());
  connection_.set_client_stub(event_logger_.client_stub());
  connection_.set_host_stub(event_logger_.host_stub());
  connection_.set_video_encode_task_runner(
      context_->video_encode_task_runner());
}

It2MeStandaloneHost::~It2MeStandaloneHost() {}

void It2MeStandaloneHost::Run() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&It2MeStandaloneHost::Connect, base::Unretained(this)));
  run_loop_.Run();
}

void It2MeStandaloneHost::StartOutputTimer() {
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(1),
      base::Bind(&OutputFakeConnectionEventLogger, std::cref(event_logger_)));
}

void It2MeStandaloneHost::Connect() {
  DesktopEnvironmentOptions options =
      DesktopEnvironmentOptions::CreateDefault();
  options.set_enable_user_interface(false);
  session_.reset(new ClientSession(
      &handler_, std::unique_ptr<protocol::ConnectionToClient>(&connection_),
      &factory_, options, base::TimeDelta(),
      scoped_refptr<protocol::PairingRegistry>(),
      std::vector<HostExtension*>()));
  session_->OnConnectionAuthenticated();
  session_->OnConnectionChannelsConnected();
  session_->CreateMediaStreams();
}

}  // namespace test
}  // namespace remoting
