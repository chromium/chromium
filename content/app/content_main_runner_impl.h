// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_CONTENT_MAIN_RUNNER_IMPL_H_
#define CONTENT_APP_CONTENT_MAIN_RUNNER_IMPL_H_

#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/startup_data_impl.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/common/main_function_params.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

namespace base {
class AtExitManager;
}  // namespace base

namespace discardable_memory {
class DiscardableSharedMemoryManager;
}

namespace content {
class MojoIpcSupport;

class ContentMainRunnerImpl : public ContentMainRunner {
 public:
  static std::unique_ptr<ContentMainRunnerImpl> Create();

  ContentMainRunnerImpl();

  ContentMainRunnerImpl(const ContentMainRunnerImpl&) = delete;
  ContentMainRunnerImpl& operator=(const ContentMainRunnerImpl&) = delete;

  ~ContentMainRunnerImpl() override;

  int TerminateForFatalInitializationError();

  // ContentMainRunner:
  int Initialize(ContentMainParams params) override;
  void ReInitializeParams(ContentMainParams new_params) override;
  int Run() override;
  void Shutdown() override;

 private:
  int RunBrowser(MainFunctionParams main_function_params,
                 bool start_minimal_browser);

  bool is_browser_main_loop_started_ = false;

  std::unique_ptr<discardable_memory::DiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;
  std::unique_ptr<MojoIpcSupport> mojo_ipc_support_;

  // True if the runner has been initialized.
  bool is_initialized_ = false;

  // True if the runner has been shut down.
  bool is_shutdown_ = false;

  // Set to true if this content process's main function should enable startup
  // tracing after initializing Mojo.
  bool needs_startup_tracing_after_mojo_init_ = false;

  // The delegate will outlive this object.
  raw_ptr<ContentMainDelegate> delegate_ = nullptr;

  std::unique_ptr<base::AtExitManager> exit_manager_;

  // Received in Initialize(), handed-off in Run().
  std::optional<ContentMainParams> content_main_params_;
};

}  // namespace content

#endif  // CONTENT_APP_CONTENT_MAIN_RUNNER_IMPL_H_
