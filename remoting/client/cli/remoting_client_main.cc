// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "mojo/core/embedder/embedder.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/cli/logging_frame_consumer.h"
#include "remoting/client/common/remoting_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

int main(int argc, char const* argv[]) {
  base::AtExitManager exitManager;
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  auto* command_line = base::CommandLine::ForCurrentProcess();

  constexpr std::string_view kSupportAccessCodeSwitch = "support_access_code";
  constexpr std::string_view kAccessTokenSwitch = "access_token";
  constexpr std::string_view kUserEmailSwitch = "user_email";

  if (!command_line->HasSwitch(kSupportAccessCodeSwitch)) {
    LOG(ERROR) << kSupportAccessCodeSwitch << " arg is missing";
  } else if (!command_line->HasSwitch(kAccessTokenSwitch)) {
    LOG(ERROR) << kAccessTokenSwitch << " arg is missing";
  } else if (!command_line->HasSwitch(kUserEmailSwitch)) {
    LOG(ERROR) << kUserEmailSwitch << " arg is missing";
  }

  auto support_access_code =
      command_line->GetSwitchValueASCII(kSupportAccessCodeSwitch);
  auto access_token = command_line->GetSwitchValueASCII(kAccessTokenSwitch);
  auto user_email = command_line->GetSwitchValueASCII(kUserEmailSwitch);

  if (support_access_code.empty() || access_token.empty() ||
      user_email.empty()) {
    return -1;
  } else if (support_access_code.length() != 12) {
    LOG(ERROR) << kSupportAccessCodeSwitch << " value must be 12 digits long.";
    return -1;
  }

  // Need to prime the client OS version value for linux to prevent IO on the
  // network thread. base::GetLinuxDistro() caches the result.
  base::GetLinuxDistro();

  mojo::core::Init();
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("RemotingClient");

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      new remoting::URLRequestContextGetter(io_task_executor.task_runner()));
  network::TransitionalURLLoaderFactoryOwner url_loader_factory_owner(
      url_request_context_getter, /*is_trusted=*/false);

  base::RunLoop run_loop;

  remoting::LoggingFrameConsumer frame_consumer;

  remoting::RemotingClient remoting_client(
      base::BindPostTask(io_task_executor.task_runner(),
                         run_loop.QuitClosure()),
      &frame_consumer, url_loader_factory_owner.GetURLLoaderFactory());

  remoting_client.StartSession(support_access_code, {access_token, user_email});

  // Allow the client to remain connected for a while before disconnecting.
  io_task_executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&remoting::RemotingClient::StopSession,
                     base::Unretained(&remoting_client)),
      base::Seconds(20));

  run_loop.Run();

  // Block until tasks blocking shutdown have completed their execution.
  base::ThreadPoolInstance::Get()->Shutdown();

  return 0;
}
