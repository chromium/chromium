// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
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

int g_read_fd = 0;
int g_write_fd = 0;

bool CreatePipe() {
  CHECK(!g_read_fd);
  CHECK(!g_write_fd);

  int pipe_fd[2];
  int result = pipe(pipe_fd);
  if (result < 0) {
    PLOG(ERROR) << "Could not create signal pipe";
    return false;
  }

  g_read_fd = pipe_fd[0];
  g_write_fd = pipe_fd[1];

  return true;
}

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
    shutdown_callback_ = std::move(shutdown_callback);

    // We cannot just PostTask from a signal handler, so route the signal
    // through a pipe.
    CHECK(CreatePipe());

    file_descriptor_watcher_controller_ =
        base::FileDescriptorWatcher::WatchReadable(
            g_read_fd,
            base::BindRepeating(
                &BrowserShutdownHandler::OnFileCanReadWithoutBlocking,
                base::Unretained(this)));
  }

  // This is called whenever data is available in |g_read_fd|.
  void OnFileCanReadWithoutBlocking() {
    int pipe_data;
    if (HANDLE_EINTR(read(g_read_fd, &pipe_data, sizeof(pipe_data))) > 0) {
      Shutdown(pipe_data);
    }
  }

  void Shutdown(int signal) {
    if (shutdown_callback_) {
      int exit_code = 0x80u + signal;
      std::move(shutdown_callback_).Run(exit_code);
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

    // Send signal number through the pipe.
    int pipe_data = signal;
    std::ignore = write(g_write_fd, &pipe_data, sizeof(pipe_data));
  }

  ShutdownCallback shutdown_callback_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      file_descriptor_watcher_controller_;
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
