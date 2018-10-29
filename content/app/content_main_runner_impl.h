// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_CONTENT_MAIN_RUNNER_IMPL_H_
#define CONTENT_APP_CONTENT_MAIN_RUNNER_IMPL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "content/browser/startup_data_impl.h"
#include "content/public/app/content_main.h"
#include "content/public/app/content_main_runner.h"
#include "content/public/common/content_client.h"

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#elif defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif  // OS_WIN

namespace base {
class AtExitManager;
}  // namespace base

namespace content {
class ContentMainDelegate;
struct ContentMainParams;

class ContentMainRunnerImpl : public ContentMainRunner {
 public:
  static ContentMainRunnerImpl* Create();

  ContentMainRunnerImpl();
  ~ContentMainRunnerImpl() override;

  int TerminateForFatalInitializationError();

  // ContentMainRunner:
  int Initialize(const ContentMainParams& params) override;
  int Run(bool start_service_manager_only) override;
  void Shutdown() override;

 private:
  // True if the runner has been initialized.
  bool is_initialized_ = false;

  // True if the runner has been shut down.
  bool is_shutdown_ = false;

  // True if basic startup was completed.
  bool completed_basic_startup_ = false;

  // Used if the embedder doesn't set one.
  ContentClient empty_content_client_;

  // The delegate will outlive this object.
  ContentMainDelegate* delegate_ = nullptr;

  std::unique_ptr<base::AtExitManager> exit_manager_;

#if defined(OS_WIN)
  sandbox::SandboxInterfaceInfo sandbox_info_;
#elif defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool* autorelease_pool_ = nullptr;
#endif

  base::Closure* ui_task_ = nullptr;

  CreatedMainPartsClosure* created_main_parts_closure_ = nullptr;

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
  std::unique_ptr<base::MessageLoop> main_message_loop_;

  std::unique_ptr<StartupDataImpl> startup_data_;

  std::unique_ptr<base::FieldTrialList> field_trial_list_;
#endif  // !defined(CHROME_MULTIPLE_DLL_CHILD)

  DISALLOW_COPY_AND_ASSIGN(ContentMainRunnerImpl);
};

}  // namespace content

#endif  // CONTENT_APP_CONTENT_MAIN_RUNNER_IMPL_H_
