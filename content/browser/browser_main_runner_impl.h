// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_MAIN_RUNNER_IMPL_H_
#define CONTENT_BROWSER_BROWSER_MAIN_RUNNER_IMPL_H_

#include <memory>

#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_runner.h"

#if BUILDFLAG(IS_WIN)
namespace ui {
class ScopedOleInitializer;
}
#endif

namespace content {

class BrowserMainLoop;

class BrowserMainRunnerImpl : public BrowserMainRunner {
 public:
  static std::unique_ptr<BrowserMainRunnerImpl> Create();

  BrowserMainRunnerImpl();

  BrowserMainRunnerImpl(const BrowserMainRunnerImpl&) = delete;
  BrowserMainRunnerImpl& operator=(const BrowserMainRunnerImpl&) = delete;

  ~BrowserMainRunnerImpl() override;

  // BrowserMainRunner:
  int Initialize(MainFunctionParams parameters) override;
#if BUILDFLAG(IS_ANDROID)
  void SynchronouslyFlushStartupTasks() override;
#endif
  int Run() override;
  void Shutdown() override;

 private:
  // True if we have started to initialize the runner.
  bool initialization_started_;

  // True if the runner has been shut down.
  bool is_shutdown_;

  // Prevents execution of ThreadPool tasks from the moment content is
  // entered. Handed off to |main_loop_| later so it can decide when to release
  // worker threads again.
  std::unique_ptr<base::ThreadPoolInstance::ScopedExecutionFence>
      scoped_execution_fence_;

  std::unique_ptr<BrowserMainLoop> main_loop_;
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<ui::ScopedOleInitializer> ole_initializer_;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_MAIN_RUNNER_IMPL_H_
