// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include "headless/app/headless_shell_switches.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_devtools.h"
#include "headless/lib/browser/headless_screen.h"

#if defined(HEADLESS_USE_PREFS)
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service_factory.h"
#endif

namespace headless {

namespace {

#if defined(HEADLESS_USE_PREFS)
const base::FilePath::CharType kLocalStateFilename[] =
    FILE_PATH_LITERAL("Local State");
#endif

}  // namespace

HeadlessBrowserMainParts::HeadlessBrowserMainParts(
    const content::MainFunctionParams& parameters,
    HeadlessBrowserImpl* browser)
    : parameters_(parameters), browser_(browser) {}

HeadlessBrowserMainParts::~HeadlessBrowserMainParts() = default;

void HeadlessBrowserMainParts::PreMainMessageLoopRun() {
#if defined(HEADLESS_USE_PREFS)
  CreatePrefService();
#endif
  if (browser_->options()->DevtoolsServerEnabled()) {
    StartLocalDevToolsHttpHandler(browser_);
    devtools_http_handler_started_ = true;
  }
  browser_->PlatformInitialize();
  browser_->RunOnStartCallback();

  if (parameters_.ui_task) {
    std::move(*parameters_.ui_task).Run();
    delete parameters_.ui_task;
    run_message_loop_ = false;
  }
}

void HeadlessBrowserMainParts::PreDefaultMainMessageLoopRun(
    base::OnceClosure quit_closure) {
  quit_main_message_loop_ = std::move(quit_closure);
}

bool HeadlessBrowserMainParts::MainMessageLoopRun(int* result_code) {
  return !run_message_loop_;
}

void HeadlessBrowserMainParts::PostMainMessageLoopRun() {
  if (devtools_http_handler_started_) {
    StopLocalDevToolsHttpHandler();
    devtools_http_handler_started_ = false;
  }
#if defined(HEADLESS_USE_PREFS)
  if (local_state_)
    local_state_->CommitPendingWrite();
#endif
}

void HeadlessBrowserMainParts::QuitMainMessageLoop() {
  if (quit_main_message_loop_)
    std::move(quit_main_message_loop_).Run();
}

#if defined(HEADLESS_USE_PREFS)
void HeadlessBrowserMainParts::CreatePrefService() {
  if (browser_->options()->user_data_dir.empty()) {
    LOG(WARNING) << "Cannot create Pref Service with no user data dir.";
    return;
  }

  base::FilePath local_state_file =
      browser_->options()->user_data_dir.Append(kLocalStateFilename);
  auto pref_store = base::MakeRefCounted<JsonPrefStore>(local_state_file);
  auto result = pref_store->ReadPrefs();
  CHECK(result == JsonPrefStore::PREF_READ_ERROR_NONE ||
        result == JsonPrefStore::PREF_READ_ERROR_NO_FILE);

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
#if defined(OS_WIN)
  OSCrypt::RegisterLocalPrefs(pref_registry.get());
#endif

  PrefServiceFactory factory;
  factory.set_user_prefs(pref_store);
  local_state_ = factory.Create(std::move(pref_registry));

#if defined(OS_WIN)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCookieEncryption)) {
    if (!OSCrypt::Init(local_state_.get()))
      LOG(ERROR) << "Failed to initialize OSCrypt";
  }
#endif
}
#endif  // defined(HEADLESS_USE_PREFS)

}  // namespace headless
