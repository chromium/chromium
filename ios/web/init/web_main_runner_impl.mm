// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/init/web_main_runner_impl.h"

#import "base/check.h"
#import "base/i18n/icu_util.h"
#import "ios/web/init/web_main_loop.h"
#import "ios/web/public/init/ios_global_state.h"
#import "ios/web/public/navigation/url_schemes.h"
#import "ios/web/web_thread_impl.h"
#import "mojo/core/embedder/embedder.h"
#import "ui/base/ui_base_paths.h"

namespace web {

WebMainRunnerImpl::WebMainRunnerImpl()
    : is_initialized_(false),
      is_shutdown_(false),
      completed_basic_startup_(false),
      delegate_(nullptr) {}

WebMainRunnerImpl::~WebMainRunnerImpl() {
  if (is_initialized_ && !is_shutdown_) {
    ShutDown();
  }
}

int WebMainRunnerImpl::Initialize(WebMainParams params) {
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

  // TODO(crbug.com/41460421): Should we instead require that all embedders call
  // SetWebClient()?
  if (!GetWebClient()) {
    SetWebClient(&empty_web_client_);
  }

  RegisterWebSchemes();
  ui::RegisterPathProvider();

  CHECK(base::i18n::InitializeICU());

  ////////////////////////////////////////////////////////////
  //  BrowserMainRunnerImpl::Initialize()

  main_loop_.reset(new WebMainLoop());
  main_loop_->Init();
  main_loop_->EarlyInitialization();
  main_loop_->CreateMainMessageLoop();
  main_loop_->CreateStartupTasks();
  int result_code = main_loop_->GetResultCode();
  if (result_code > 0) {
    return result_code;
  }

  // Return -1 to indicate no early termination.
  return -1;
}

void WebMainRunnerImpl::ShutDown() {
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
  if (delegate_) {
    delegate_->ProcessExiting();
  }

  ios_global_state::DestroyAtExitManager();

  delegate_ = nullptr;
  is_shutdown_ = true;
}

}  // namespace web
