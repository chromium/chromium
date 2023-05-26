// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include <memory.h>
#include <stdio.h>

#include "base/debug/alias.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/headless/clipboard/headless_clipboard.h"
#include "components/headless/select_file_dialog/headless_select_file_dialog.h"
#include "content/public/common/result_codes.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_devtools.h"
#include "headless/lib/browser/headless_screen.h"
#include "headless/public/switches.h"

#if defined(HEADLESS_USE_PREFS)
#include "components/os_crypt/sync/os_crypt.h"  // nogncheck
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service_factory.h"
#endif

#if defined(HEADLESS_USE_POLICY)
#include "components/headless/policy/headless_mode_policy.h"  // nogncheck
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "headless/lib/browser/policy/headless_policies.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/command_line.h"
#endif

namespace headless {

namespace {

#if defined(HEADLESS_USE_PREFS)
const base::FilePath::CharType kLocalStateFilename[] =
    FILE_PATH_LITERAL("Local State");
#endif

}  // namespace

HeadlessBrowserMainParts::HeadlessBrowserMainParts(HeadlessBrowserImpl* browser)
    : browser_(browser) {}

HeadlessBrowserMainParts::~HeadlessBrowserMainParts() = default;

int HeadlessBrowserMainParts::PreMainMessageLoopRun() {
#if defined(HEADLESS_USE_PREFS)
  CreatePrefService();
#endif
  MaybeStartLocalDevToolsHttpHandler();
  SetHeadlessClipboardForCurrentThread();
  browser_->PlatformInitialize();
  browser_->RunOnStartCallback();
  HeadlessSelectFileDialogFactory::SetUp();
  return content::RESULT_CODE_NORMAL_EXIT;
}

void HeadlessBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  quit_main_message_loop_ = run_loop->QuitClosure();
}

void HeadlessBrowserMainParts::PostMainMessageLoopRun() {
  // HeadlessBrowserImpl::Shutdown() is supposed to remove all browser contexts
  // and therefore all associated web contents, however crbug.com/1342152
  // implies it may not be happening.
  CHECK_EQ(0U, browser_->GetAllBrowserContexts().size());
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

#if BUILDFLAG(IS_MAC)
device::GeolocationManager* HeadlessBrowserMainParts::GetGeolocationManager() {
  return geolocation_manager_.get();
}

void HeadlessBrowserMainParts::SetGeolocationManagerForTesting(
    std::unique_ptr<device::GeolocationManager> fake_geolocation_manager) {
  geolocation_manager_ = std::move(fake_geolocation_manager);
}
#endif

void HeadlessBrowserMainParts::QuitMainMessageLoop() {
  if (quit_main_message_loop_)
    std::move(quit_main_message_loop_).Run();
}

void HeadlessBrowserMainParts::MaybeStartLocalDevToolsHttpHandler() {
  if (!browser_->options()->DevtoolsServerEnabled())
    return;

#if defined(HEADLESS_USE_POLICY)
  const PrefService* pref_service = browser_->GetPrefs();
  if (!IsRemoteDebuggingAllowed(pref_service)) {
    // Follow content/browser/devtools/devtools_http_handler.cc that reports its
    // remote debugging port on stderr for symmetry.
    fputs("\nDevTools remote debugging is disallowed by the system admin.\n",
          stderr);
    fflush(stderr);
    return;
  }
#endif

  StartLocalDevToolsHttpHandler(browser_);
  devtools_http_handler_started_ = true;
}

#if defined(HEADLESS_USE_PREFS)
void HeadlessBrowserMainParts::CreatePrefService() {
  scoped_refptr<PersistentPrefStore> pref_store;
  if (browser_->options()->user_data_dir.empty()) {
    pref_store = base::MakeRefCounted<InMemoryPrefStore>();
  } else {
    base::FilePath local_state_file =
        browser_->options()->user_data_dir.Append(kLocalStateFilename);
    pref_store = base::MakeRefCounted<JsonPrefStore>(
        local_state_file,
        /*pref_filter=*/nullptr,
        /*file_task_runner=*/
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
        /*read_only=*/true);
    auto result = pref_store->ReadPrefs();
    base::debug::Alias(&result);
    if (result != JsonPrefStore::PREF_READ_ERROR_NONE) {
      CHECK_EQ(result, JsonPrefStore::PREF_READ_ERROR_NO_FILE);
    }
  }

  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
#if BUILDFLAG(IS_WIN)
  OSCrypt::RegisterLocalPrefs(pref_registry.get());
#endif

  PrefServiceFactory factory;

#if defined(HEADLESS_USE_POLICY)
  RegisterHeadlessPrefs(pref_registry.get());

  policy_connector_ =
      std::make_unique<policy::HeadlessBrowserPolicyConnector>();

  factory.set_managed_prefs(
      policy_connector_->CreatePrefStore(policy::POLICY_LEVEL_MANDATORY));

  BrowserContextDependencyManager::GetInstance()
      ->RegisterProfilePrefsForServices(pref_registry.get());
#endif  // defined(HEADLESS_USE_POLICY)

  factory.set_user_prefs(pref_store);
  local_state_ = factory.Create(std::move(pref_registry));

#if BUILDFLAG(IS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableCookieEncryption) &&
      OSCrypt::InitWithExistingKey(local_state_.get()) != OSCrypt::kSuccess) {
    command_line->AppendSwitch(switches::kDisableCookieEncryption);
  }
#endif  // BUILDFLAG(IS_WIN)
}
#endif  // defined(HEADLESS_USE_PREFS)

#if defined(HEADLESS_USE_POLICY)

policy::PolicyService* HeadlessBrowserMainParts::GetPolicyService() {
  return policy_connector_ ? policy_connector_->GetPolicyService() : nullptr;
}

#endif  // defined(HEADLESS_USE_POLICY)

}  // namespace headless
