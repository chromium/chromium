// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_CONTENT_MAIN_RUNNER_IMPL_H_
#define CONTENT_APP_CONTENT_MAIN_RUNNER_IMPL_H_

#include <memory>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/browser/startup_data_impl.h"
#include "content/common/content_export.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/common/main_function_params.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // Unregisters UI thread from hang watching on destruction.
  // NOTE: The thread should be unregistered before HangWatcher stops so this
  // member must be after |hang_watcher|.
  base::ScopedClosureRunner unregister_thread_closure_;

  std::unique_ptr<discardable_memory::DiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;
  std::unique_ptr<MojoIpcSupport> mojo_ipc_support_;

  // True if the runner has been initialized.
  bool is_initialized_ = false;

  // True if the runner has been shut down.
  bool is_shutdown_ = false;

  // True if basic startup was completed.
  bool completed_basic_startup_ = false;

  // The delegate will outlive this object.
  raw_ptr<ContentMainDelegate> delegate_ = nullptr;

  std::unique_ptr<base::AtExitManager> exit_manager_;

  // Received in Initialize(), handed-off in Run().
  absl::optional<ContentMainParams> content_main_params_;
};

// The BrowserTestBase on Android does not call ContentMain(). It tries instead
// to reproduce it more or less accurately. This requires to use
// GetContentMainDelegateForTesting() and GetContentClientForTesting().
// BrowserTestBase is implemented in content/public and GetContentClient() is
// only available to the implementation of content. Hence these functions.
#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT ContentMainDelegate* GetContentMainDelegateForTesting();
#endif

}  // namespace content

#endif  // CONTENT_APP_CONTENT_MAIN_RUNNER_IMPL_H_
