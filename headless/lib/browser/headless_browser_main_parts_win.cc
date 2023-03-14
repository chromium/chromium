// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include <Windows.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "headless/lib/browser/headless_browser_impl.h"

namespace headless {

namespace {

class BrowserShutdownHandler {
 public:
  typedef base::OnceCallback<void(int)> ShutdownCallback;

  BrowserShutdownHandler(const BrowserShutdownHandler&) = delete;
  BrowserShutdownHandler& operator=(const BrowserShutdownHandler&) = delete;

  static void Install(ShutdownCallback shutdown_callback) {
    GetInstance().Init(std::move(shutdown_callback));

    PCHECK(::SetConsoleCtrlHandler(ConsoleCtrlHandler, true) != 0);
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

  bool Shutdown(DWORD ctrl_type) {
    // If the callback is already consumed, let the default handler do its
    // thing.
    if (!shutdown_callback_) {
      return false;
    }

    DCHECK_LT(ctrl_type, 0x7fu);
    int exit_code = 0x80u + ctrl_type;
    if (!task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(shutdown_callback_), exit_code))) {
      RAW_LOG(WARNING, "No valid task runner, exiting ungracefully.");
      return false;
    }

    return true;
  }

  static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl_type) {
    return GetInstance().Shutdown(ctrl_type);
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  ShutdownCallback shutdown_callback_;
};

}  // namespace

void HeadlessBrowserMainParts::PostCreateMainMessageLoop() {
  BrowserShutdownHandler::Install(base::BindOnce(
      &HeadlessBrowserImpl::ShutdownWithExitCode, browser_->GetWeakPtr()));
}

}  // namespace headless
