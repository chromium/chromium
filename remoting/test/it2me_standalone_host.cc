// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/it2me_standalone_host.h"

#include <functional>
#include <iostream>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/session_policies.h"
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
      context_(ChromotingHostContext::Create(new AutoThreadTaskRunner(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          run_loop_.QuitClosure()))),
      main_task_runner_(context_->file_task_runner()),
      factory_(main_task_runner_,
               context_->video_capture_task_runner(),
               context_->input_task_runner(),
               context_->ui_task_runner()),
      connection_(base::WrapUnique(new testing::NiceMock<MockSession>())),
      session_jid_(kSessionJid),
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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
  timer_.Start(FROM_HERE, base::Seconds(1),
               base::BindRepeating(&OutputFakeConnectionEventLogger,
                                   std::cref(event_logger_)));
}

void It2MeStandaloneHost::Connect() {
  DesktopEnvironmentOptions options =
      DesktopEnvironmentOptions::CreateDefault();
  options.set_enable_user_interface(false);
  session_ = std::make_unique<ClientSession>(
      &handler_, std::unique_ptr<protocol::ConnectionToClient>(&connection_),
      &factory_, options, scoped_refptr<protocol::PairingRegistry>(),
      std::vector<raw_ptr<HostExtension, VectorExperimental>>(),
      &local_session_policies_provider_);
  session_->OnConnectionAuthenticated(nullptr);
  session_->OnConnectionChannelsConnected();
  session_->CreateMediaStreams();
}

}  // namespace test
}  // namespace remoting
