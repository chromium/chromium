// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include <signal.h>
#include <unistd.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "headless/lib/browser/headless_browser_impl.h"

#if BUILDFLAG(IS_LINUX)
#include "base/command_line.h"
#include "components/os_crypt/sync/key_storage_config_linux.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "headless/public/switches.h"

#if defined(USE_DBUS)
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#endif

#endif  // BUILDFLAG(IS_LINUX)

namespace headless {

namespace {

class BrowserShutdownHandler {
 public:
  typedef base::OnceCallback<void(int)> ShutdownCallback;

  BrowserShutdownHandler(const BrowserShutdownHandler&) = delete;
  BrowserShutdownHandler& operator=(const BrowserShutdownHandler&) = delete;

  static void Install(ShutdownCallback shutdown_callback) {
    GetInstance().Init(std::move(shutdown_callback));

    // We need to handle SIGTERM, because that is how many POSIX-based distros
    // ask processes to quit gracefully at shutdown time.
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIGTERMHandler;
    PCHECK(sigaction(SIGTERM, &action, nullptr) == 0);

    // Also handle SIGINT - when the user terminates the browser via Ctrl+C. If
    // the browser process is being debugged, GDB will catch the SIGINT first.
    action.sa_handler = SIGINTHandler;
    PCHECK(sigaction(SIGINT, &action, nullptr) == 0);

    // And SIGHUP, for when the terminal disappears. On shutdown, many Linux
    // distros send SIGHUP, SIGTERM, and then SIGKILL.
    action.sa_handler = SIGHUPHandler;
    PCHECK(sigaction(SIGHUP, &action, nullptr) == 0);
  }

 private:
  friend class base::NoDestructor<BrowserShutdownHandler>;

  BrowserShutdownHandler() = default;
  ~BrowserShutdownHandler() = default;

  static BrowserShutdownHandler& GetInstance() {
    static base::NoDestructor<BrowserShutdownHandler> instance;
    return *instance;
  }

  void Init(ShutdownCallback shutdown_callback) {
    task_runner_ = content::GetUIThreadTaskRunner({});
    shutdown_callback_ = std::move(shutdown_callback);
  }

  void Shutdown(int signal) {
    if (shutdown_callback_) {
      int exit_code = 0x80u + signal;
      if (!task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(shutdown_callback_), exit_code))) {
        RAW_LOG(WARNING, "No valid task runner, exiting ungracefully.");
        kill(getpid(), signal);
      }
    }
  }

  static void SIGHUPHandler(int signal) {
    RAW_CHECK(signal == SIGHUP);
    ShutdownHandler(signal);
  }

  static void SIGINTHandler(int signal) {
    RAW_CHECK(signal == SIGINT);
    ShutdownHandler(signal);
  }

  static void SIGTERMHandler(int signal) {
    RAW_CHECK(signal == SIGTERM);
    ShutdownHandler(signal);
  }

  static void ShutdownHandler(int signal) {
    // Reinstall the default handler. We have only one shot at graceful
    // shutdown.
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_DFL;
    RAW_CHECK(sigaction(signal, &action, nullptr) == 0);

    GetInstance().Shutdown(signal);
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  ShutdownCallback shutdown_callback_;
};

}  // namespace

#if BUILDFLAG(IS_LINUX)
constexpr char kProductName[] = "HeadlessChrome";
#endif

void HeadlessBrowserMainParts::PostCreateMainMessageLoop() {
  BrowserShutdownHandler::Install(base::BindOnce(
      &HeadlessBrowserImpl::ShutdownWithExitCode, browser_->GetWeakPtr()));

#if BUILDFLAG(IS_LINUX)

#if defined(USE_DBUS)
  bluez::BluezDBusManager::Initialize(/*system_bus=*/nullptr);
#endif

  // Set up crypt config. This needs to be done before anything starts the
  // network service, as the raw encryption key needs to be shared with the
  // network service for encrypted cookie storage.
  std::unique_ptr<os_crypt::Config> config =
      std::make_unique<os_crypt::Config>();
  // Forward to os_crypt the flag to use a specific password store.
  config->store = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kPasswordStore);
  // Use a default product name
  config->product_name = kProductName;
  // OSCrypt can be disabled in a special settings file, but headless doesn't
  // need to support that.
  config->should_use_preference = false;
  config->user_data_path = base::FilePath();
  OSCrypt::SetConfig(std::move(config));
#endif  // BUILDFLAG(IS_LINUX)
}

}  // namespace headless
