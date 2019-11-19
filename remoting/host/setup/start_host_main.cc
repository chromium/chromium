// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/start_host_main.h"

#include <stddef.h>
#include <stdio.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/logging.h"
#include "remoting/base/service_urls.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/pin_validator.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

#if defined(OS_POSIX)
#include <termios.h>
#include <unistd.h>
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
#include "base/process/process_info.h"
#endif  // defined(OS_WIN)

namespace remoting {

namespace {

// True if the host was started successfully.
bool g_started = false;

base::SingleThreadTaskExecutor* g_main_thread_task_executor = nullptr;

// The active RunLoop.
base::RunLoop* g_active_run_loop = nullptr;

// Lets us hide the PIN that a user types.
void SetEcho(bool echo) {
#if defined(OS_WIN)
  DWORD mode;
  HANDLE console_handle = GetStdHandle(STD_INPUT_HANDLE);
  if (!GetConsoleMode(console_handle, &mode)) {
    LOG(ERROR) << "GetConsoleMode failed";
    return;
  }
  SetConsoleMode(console_handle,
                 (mode & ~ENABLE_ECHO_INPUT) | (echo ? ENABLE_ECHO_INPUT : 0));
#else
  termios term;
  tcgetattr(STDIN_FILENO, &term);
  if (echo) {
    term.c_lflag |= ECHO;
  } else {
    term.c_lflag &= ~ECHO;
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &term);
#endif  // !defined(OS_WIN)
}

// Reads a newline-terminated string from stdin.
std::string ReadString(bool no_echo) {
  if (no_echo)
    SetEcho(false);
  const int kMaxLen = 1024;
  std::string str(kMaxLen, 0);
  char* result = fgets(&str[0], kMaxLen, stdin);
  if (no_echo) {
    printf("\n");
    SetEcho(true);
  }
  if (!result)
    return std::string();
  size_t newline_index = str.find('\n');
  if (newline_index != std::string::npos)
    str[newline_index] = '\0';
  str.resize(strlen(&str[0]));
  return str;
}

// Called when the HostStarter has finished.
void OnDone(HostStarter::Result result) {
  if (!g_main_thread_task_executor->task_runner()->BelongsToCurrentThread()) {
    g_main_thread_task_executor->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&OnDone, result));
    return;
  }
  switch (result) {
    case HostStarter::START_COMPLETE:
      g_started = true;
      break;
    case HostStarter::NETWORK_ERROR:
      fprintf(stderr, "Couldn't start host: network error.\n");
      break;
    case HostStarter::OAUTH_ERROR:
      fprintf(stderr, "Couldn't start host: OAuth error.\n");
      break;
    case HostStarter::START_ERROR:
      fprintf(stderr, "Couldn't start host.\n");
      break;
  }

  g_active_run_loop->Quit();
}

}  // namespace

int StartHostMain(int argc, char** argv) {
  // google_apis::GetOAuth2ClientID/Secret need a static CommandLine.
  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // This object instance is required by Chrome code (for example,
  // FilePath, LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "RemotingHostSetup");

  mojo::core::Init();

  std::string host_name = command_line->GetSwitchValueASCII("name");
  std::string host_pin = command_line->GetSwitchValueASCII("pin");
  std::string auth_code = command_line->GetSwitchValueASCII("code");
  std::string redirect_url = command_line->GetSwitchValueASCII("redirect-url");

#if defined(OS_POSIX)
  // Check if current user is root. If it is root, then throw an error message.
  // This is because start_host should be run in user mode.
  if (geteuid() == 0) {
    fprintf(stderr, "Refusing to run %s as root.", argv[0]);
    return 1;
  }
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
  // The tool must be run elevated on Windows so the host has access to the
  // directories used to store the configuration JSON files.
  if (!base::IsCurrentProcessElevated()) {
    fprintf(stderr, "Error: %s must be run as an elevated process.", argv[0]);
    return 1;
  }
#endif  // defined(OS_WIN)

  if (command_line->HasSwitch("help") || command_line->HasSwitch("h") ||
      command_line->HasSwitch("?") || !command_line->GetArgs().empty()) {
    fprintf(stderr,
            "Usage: %s [--name=<hostname>] [--code=<auth-code>] [--pin=<PIN>] "
            "[--redirect-url=<redirectURL>]\n",
            argv[0]);
    return 1;
  }

  if (auth_code.empty() || redirect_url.empty()) {
    fprintf(stdout,
            "You need a web browser to use this command. Please visit\n");
    fprintf(stdout,
            "https://remotedesktop.google.com/headless for instructions.\n");
    return 1;
  }

  if (host_name.empty()) {
    fprintf(stdout, "Enter a name for this computer: ");
    fflush(stdout);
    host_name = ReadString(false);
  }

  if (host_pin.empty()) {
    while (true) {
      fprintf(stdout, "Enter a PIN of at least six digits: ");
      fflush(stdout);
      host_pin = ReadString(true);
      if (!remoting::IsPinValid(host_pin)) {
        fprintf(stdout,
                "Please use a PIN consisting of at least six digits.\n");
        fflush(stdout);
        continue;
      }
      std::string host_pin_confirm;
      fprintf(stdout, "Enter the same PIN again: ");
      fflush(stdout);
      host_pin_confirm = ReadString(true);
      if (host_pin != host_pin_confirm) {
        fprintf(stdout, "You entered different PINs.\n");
        fflush(stdout);
        continue;
      }
      break;
    }
  } else {
    if (!remoting::IsPinValid(host_pin)) {
      fprintf(stderr, "Please use a PIN consisting of at least six digits.\n");
      return 1;
    }
  }

  // Provide SingleThreadTaskExecutor and threads for the
  // URLRequestContextGetter.
  base::SingleThreadTaskExecutor main_thread_task_executor;
  g_main_thread_task_executor = &main_thread_task_executor;
  base::Thread::Options io_thread_options(base::MessagePumpType::IO, 0);
  base::Thread io_thread("IO thread");
  io_thread.StartWithOptions(io_thread_options);

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      new remoting::URLRequestContextGetter(io_thread.task_runner()));
  network::TransitionalURLLoaderFactoryOwner url_loader_factory_owner(
      url_request_context_getter);

  net::URLFetcher::SetIgnoreCertificateRequests(true);

  // Start the host.
  std::unique_ptr<HostStarter> host_starter(HostStarter::Create(
      remoting::ServiceUrls::GetInstance()->remoting_server_endpoint(),
      url_loader_factory_owner.GetURLLoaderFactory()));
  host_starter->StartHost(host_name, host_pin,
                          /*consent_to_data_collection=*/true, auth_code,
                          redirect_url, base::Bind(&OnDone));

  // Run the task executor until the StartHost completion callback.
  base::RunLoop run_loop;
  g_active_run_loop = &run_loop;
  run_loop.Run();

  g_main_thread_task_executor = nullptr;
  g_active_run_loop = nullptr;

  // Destroy the HostStarter and URLRequestContextGetter before stopping the
  // IO thread.
  host_starter.reset();
  url_request_context_getter = nullptr;

  io_thread.Stop();

  return g_started ? 0 : 1;
}

}  // namespace remoting
