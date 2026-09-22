// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string_view>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/memory_coordinator/dummy_memory_consumer_registry.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "mojo/core/embedder/embedder.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/cli/logging_audio_stream_consumer.h"
#include "remoting/client/cli/logging_frame_consumer.h"
#include "remoting/client/common/logging.h"
#include "remoting/client/common/remoting_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace {
constexpr base::TimeDelta kDefaultDisconnectTimeout = base::Seconds(20);
constexpr base::TimeDelta kMaxDisconnectTimeout = base::Minutes(5);
}  // namespace

int main(int argc, char const* argv[]) {
  base::ScopedMemoryConsumerRegistry<base::DummyMemoryConsumerRegistry>
      memory_consumer_registry;

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
  constexpr std::string_view kDisconnectTimeoutSwitch = "disconnect_timeout";
  constexpr std::string_view kDisconnectTimeoutSecondsSwitch =
      "disconnect_timeout_seconds";

  if (!command_line->HasSwitch(kSupportAccessCodeSwitch)) {
    LOG(ERROR) << kSupportAccessCodeSwitch << " arg is missing";
  } else if (!command_line->HasSwitch(kAccessTokenSwitch)) {
    LOG(ERROR) << kAccessTokenSwitch << " arg is missing";
  } else if (!command_line->HasSwitch(kUserEmailSwitch)) {
    LOG(ERROR) << kUserEmailSwitch << " arg is missing";
  }

  std::vector<std::string> support_access_codes = base::SplitString(
      command_line->GetSwitchValueASCII(kSupportAccessCodeSwitch), ",",
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  auto access_token = command_line->GetSwitchValueASCII(kAccessTokenSwitch);
  auto user_email = command_line->GetSwitchValueASCII(kUserEmailSwitch);

  if (support_access_codes.empty() || access_token.empty() ||
      user_email.empty()) {
    return -1;
  }
  for (auto& code : support_access_codes) {
    if (code.length() != 12) {
      LOG(ERROR) << "Invalid support access code found (must be 12 digits): "
                 << code;
      return -1;
    }
  }

  base::TimeDelta disconnect_timeout = kDefaultDisconnectTimeout;
  std::string timeout_str =
      command_line->HasSwitch(kDisconnectTimeoutSwitch)
          ? command_line->GetSwitchValueASCII(kDisconnectTimeoutSwitch)
          : command_line->GetSwitchValueASCII(kDisconnectTimeoutSecondsSwitch);
  if (!timeout_str.empty()) {
    int timeout_seconds = 0;
    if (!base::StringToInt(timeout_str, &timeout_seconds) ||
        timeout_seconds <= 0) {
      LOG(ERROR) << "Invalid disconnect timeout provided: " << timeout_str;
      return -1;
    }
    disconnect_timeout = base::Seconds(timeout_seconds);
    if (disconnect_timeout > kMaxDisconnectTimeout) {
      LOG(WARNING) << "Disconnect timeout exceeds max of "
                   << kMaxDisconnectTimeout << ", clamping.";
      disconnect_timeout = kMaxDisconnectTimeout;
    }
  }

  // Need to prime the client OS version value for linux to prevent IO on the
  // network thread. base::GetLinuxDistro() caches the result.
  base::GetLinuxDistro();

  mojo::core::Init();
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("RemotingClient");

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      new remoting::URLRequestContextGetter(io_task_executor.task_runner()));
  std::optional<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner;
#if !defined(NDEBUG)
  // Debug builds default to sandbox endpoints which require client certificate
  // authentication (mTLS) at the edge.
  constexpr bool kIsTrusted = true;
#else
  constexpr bool kIsTrusted = false;
#endif
  url_loader_factory_owner.emplace(url_request_context_getter,
                                   /*is_trusted=*/kIsTrusted);

  for (auto& code : support_access_codes) {
    CLIENT_LOG << "Creating remoting client for support host: " << code;

    base::RunLoop run_loop;

    remoting::LoggingFrameConsumer frame_consumer;
    remoting::LoggingAudioStreamConsumer audio_consumer;

    auto remoting_client = std::make_unique<remoting::RemotingClient>(
        base::BindPostTask(io_task_executor.task_runner(),
                           run_loop.QuitClosure()),
        &frame_consumer, audio_consumer.GetWeakPtr(),
        url_loader_factory_owner->GetURLLoaderFactory());

    CLIENT_LOG << "Starting session for support host: " << code;
    remoting_client->StartSession(code, {access_token, user_email});

    // Allow the client to remain connected for a while before disconnecting.
    io_task_executor.task_runner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&remoting::RemotingClient::StopSession,
                       remoting_client->GetWeakPtr()),
        disconnect_timeout);

    CLIENT_LOG << "Running the session for up to " << disconnect_timeout
               << "...";
    run_loop.Run();

    CLIENT_LOG << "Tearing down remoting client for support host: " << code;
    remoting_client.reset();
  }

  url_loader_factory_owner.reset();
  url_request_context_getter.reset();
  base::RunLoop().RunUntilIdle();

  return 0;
}
