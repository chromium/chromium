// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include <Windows.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "headless/lib/browser/headless_browser_impl.h"

namespace headless {

namespace {

class BrowserShutdownHandler {
 public:
  BrowserShutdownHandler(const BrowserShutdownHandler&) = delete;
  BrowserShutdownHandler& operator=(const BrowserShutdownHandler&) = delete;

  static void Install(base::OnceClosure shutdown_callback) {
    GetInstance().Init(std::move(shutdown_callback));
  }

 private:
  friend class base::NoDestructor<BrowserShutdownHandler>;

  BrowserShutdownHandler() = default;
  ~BrowserShutdownHandler() = default;

  static BrowserShutdownHandler& GetInstance() {
    static base::NoDestructor<BrowserShutdownHandler> instance;
    return *instance;
  }

  void Init(base::OnceClosure shutdown_callback) {
    shutdown_callback_ = std::move(shutdown_callback);

    PCHECK(::SetConsoleCtrlHandler(ConsoleCtrlHandler, true) != 0);
  }

  bool Shutdown() {
    // If the callback is already consumed, let the default handler do its
    // thing.
    if (!shutdown_callback_) {
      return false;
    }

    std::move(shutdown_callback_).Run();

    return true;
  }

  static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrl_type) {
    return GetInstance().Shutdown();
  }

  base::OnceClosure shutdown_callback_;
};

}  // namespace

void HeadlessBrowserMainParts::PostCreateMainMessageLoop() {
  BrowserShutdownHandler::Install(base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&HeadlessBrowserImpl::Shutdown, browser_->GetWeakPtr())));
}

}  // namespace headless
