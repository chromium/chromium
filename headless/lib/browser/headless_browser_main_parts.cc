// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include "content/public/common/result_codes.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_devtools.h"
#include "headless/lib/browser/headless_screen.h"

#if defined(HEADLESS_USE_PREFS)
#include "components/os_crypt/os_crypt.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service_factory.h"
#endif

#if defined(HEADLESS_USE_POLICY)
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "headless/lib/browser/policy/headless_mode_policy.h"
#endif

#if defined(OS_MAC)
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
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

int HeadlessBrowserMainParts::PreMainMessageLoopRun() {
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

  return content::RESULT_CODE_NORMAL_EXIT;
}

void HeadlessBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  if (run_message_loop_)
    quit_main_message_loop_ = run_loop->QuitClosure();
  else
    run_loop.reset();
}

void HeadlessBrowserMainParts::PostMainMessageLoopRun() {
  if (devtools_http_handler_started_) {
    StopLocalDevToolsHttpHandler();
    devtools_http_handler_started_ = false;
  }
#if defined(HEADLESS_USE_PREFS)
  if (local_state_) {
    local_state_->CommitPendingWrite();
    local_state_.reset(nullptr);
  }
#endif
#if defined(HEADLESS_USE_POLICY)
  if (policy_connector_) {
    policy_connector_->Shutdown();
    policy_connector_.reset(nullptr);
  }
#endif
}

#if defined(OS_MAC)
device::GeolocationManager* HeadlessBrowserMainParts::GetGeolocationManager() {
  return geolocation_manager_.get();
}
#endif

void HeadlessBrowserMainParts::QuitMainMessageLoop() {
  if (quit_main_message_loop_)
    std::move(quit_main_message_loop_).Run();
}

#if defined(HEADLESS_USE_PREFS)
void HeadlessBrowserMainParts::CreatePrefService() {
  scoped_refptr<PersistentPrefStore> pref_store;
  if (browser_->options()->user_data_dir.empty()) {
    pref_store = base::MakeRefCounted<InMemoryPrefStore>();
  } else {
    base::FilePath local_state_file =
        browser_->options()->user_data_dir.Append(kLocalStateFilename);
    pref_store = base::MakeRefCounted<JsonPrefStore>(local_state_file);
    auto result = pref_store->ReadPrefs();
    CHECK(result == JsonPrefStore::PREF_READ_ERROR_NONE ||
          result == JsonPrefStore::PREF_READ_ERROR_NO_FILE);
  }

  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
#if defined(OS_WIN)
  OSCrypt::RegisterLocalPrefs(pref_registry.get());
#endif

  PrefServiceFactory factory;

#if defined(HEADLESS_USE_POLICY)
  policy::HeadlessModePolicy::RegisterLocalPrefs(pref_registry.get());
  policy::URLBlocklistManager::RegisterProfilePrefs(pref_registry.get());

  policy_connector_ =
      std::make_unique<policy::HeadlessBrowserPolicyConnector>();

  factory.set_managed_prefs(
      policy_connector_->CreatePrefStore(policy::POLICY_LEVEL_MANDATORY));

  BrowserContextDependencyManager::GetInstance()
      ->RegisterProfilePrefsForServices(pref_registry.get());
#endif  // defined(HEADLESS_USE_POLICY)

  factory.set_user_prefs(pref_store);
  local_state_ = factory.Create(std::move(pref_registry));

#if defined(OS_WIN)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCookieEncryption)) {
    if (!OSCrypt::Init(local_state_.get()))
      LOG(ERROR) << "Failed to initialize OSCrypt";
  }
#endif  // defined(OS_WIN)
}
#endif  // defined(HEADLESS_USE_PREFS)

#if defined(HEADLESS_USE_POLICY)

policy::PolicyService* HeadlessBrowserMainParts::GetPolicyService() {
  return policy_connector_ ? policy_connector_->GetPolicyService() : nullptr;
}

#endif  // defined(HEADLESS_USE_POLICY)

}  // namespace headless
