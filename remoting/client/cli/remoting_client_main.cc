// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/client/common/remoting_client.h"

int main(int argc, char const* argv[]) {
  base::AtExitManager exitManager;
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  auto* command_line = base::CommandLine::ForCurrentProcess();

  constexpr std::string_view kSupportIdSwitch = "support_id";
  constexpr std::string_view kAccessTokenSwitch = "access_token";

  if (!command_line->HasSwitch(kSupportIdSwitch)) {
    LOG(ERROR) << kSupportIdSwitch << " arg is missing";
  } else if (!command_line->HasSwitch(kAccessTokenSwitch)) {
    LOG(ERROR) << kAccessTokenSwitch << " arg is missing";
  }

  auto support_id = command_line->GetSwitchValueASCII(kSupportIdSwitch);
  auto access_token = command_line->GetSwitchValueASCII(kAccessTokenSwitch);

  if (support_id.empty() || access_token.empty()) {
    return -1;
  }

  // Need to prime the client OS version value for linux to prevent IO on the
  // network thread. base::GetLinuxDistro() caches the result.
  base::GetLinuxDistro();

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  remoting::RemotingClient remoting_client;

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("RemotingClient");
  mojo::core::Init();

  remoting_client.StartSession(support_id, access_token);

  return 0;
}
