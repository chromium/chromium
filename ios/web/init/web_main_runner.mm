// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/init/web_main_runner.h"

#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "ios/web/init/web_main_loop.h"
#include "ios/web/public/init/ios_global_state.h"
#include "ios/web/public/navigation/url_schemes.h"
#import "ios/web/public/web_client.h"
#include "ios/web/web_thread_impl.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/ui_base_paths.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

class WebMainRunnerImpl : public WebMainRunner {
 public:
  WebMainRunnerImpl()
      : is_initialized_(false),
        is_shutdown_(false),
        completed_basic_startup_(false),
        delegate_(nullptr) {}

  ~WebMainRunnerImpl() override {
    if (is_initialized_ && !is_shutdown_) {
      ShutDown();
    }
  }

  int Initialize(WebMainParams params) override {
    ////////////////////////////////////////////////////////////////////////
    // ContentMainRunnerImpl::Initialize()
    //
    is_initialized_ = true;
    delegate_ = params.delegate;

    ios_global_state::CreateParams create_params;
    create_params.install_at_exit_manager = params.register_exit_manager;
    create_params.argc = params.argc;
    create_params.argv = params.argv;
    ios_global_state::Create(create_params);
    web::WebThreadImpl::CreateTaskExecutor();

    if (delegate_) {
      delegate_->BasicStartupComplete();
    }
    completed_basic_startup_ = true;

    mojo::core::Init();

    // TODO(crbug.com/965894): Should we instead require that all embedders call
    // SetWebClient()?
    if (!GetWebClient())
      SetWebClient(&empty_web_client_);

    RegisterWebSchemes(true);
    ui::RegisterPathProvider();

    CHECK(base::i18n::InitializeICU());

    ////////////////////////////////////////////////////////////
    //  BrowserMainRunnerImpl::Initialize()

    main_loop_.reset(new WebMainLoop());
    main_loop_->Init();
    main_loop_->EarlyInitialization();
    main_loop_->MainMessageLoopStart();
    main_loop_->CreateStartupTasks();
    int result_code = main_loop_->GetResultCode();
    if (result_code > 0)
      return result_code;

    // Return -1 to indicate no early termination.
    return -1;
  }

  void ShutDown() override {
    ////////////////////////////////////////////////////////////////////
    // BrowserMainRunner::Shutdown()
    //
    DCHECK(is_initialized_);
    DCHECK(!is_shutdown_);
    main_loop_->ShutdownThreadsAndCleanUp();
    main_loop_.reset(nullptr);

    ////////////////////////////////////////////////////////////////////
    // ContentMainRunner::Shutdown()
    //
    if (completed_basic_startup_ && delegate_) {
      delegate_->ProcessExiting();
    }

    ios_global_state::DestroyAtExitManager();

    delegate_ = nullptr;
    is_shutdown_ = true;
  }

 protected:
  // True if we have started to initialize the runner.
  bool is_initialized_;

  // True if the runner has been shut down.
  bool is_shutdown_;

  // True if basic startup was completed.
  bool completed_basic_startup_;

  // The delegate will outlive this object.
  WebMainDelegate* delegate_;

  // Used if the embedder doesn't set one.
  WebClient empty_web_client_;

  std::unique_ptr<WebMainLoop> main_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebMainRunnerImpl);
};

// static
WebMainRunner* WebMainRunner::Create() {
  return new WebMainRunnerImpl();
}

}  // namespace web
