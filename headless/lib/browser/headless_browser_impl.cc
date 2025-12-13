// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/config/linux/dbus/buildflags.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_platform_delegate.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

#if BUILDFLAG(IS_WIN)
#include "base/command_line.h"
#include "components/os_crypt/async/browser/dpapi_key_provider.h"
#include "headless/public/switches.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "components/os_crypt/async/browser/keychain_key_provider.h"
#endif

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
#include "base/command_line.h"
#include "components/os_crypt/async/browser/freedesktop_secret_key_provider.h"
#include "components/password_manager/core/browser/password_manager_switches.h"  // nogncheck
#endif

#if BUILDFLAG(IS_POSIX)
#include "components/os_crypt/async/browser/posix_key_provider.h"  // nogncheck
#endif

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

#if defined(HEADLESS_SUPPORT_FIELD_TRIALS)
#include "components/metrics/metrics_service.h"                // nogncheck
#include "components/variations/service/variations_service.h"  // nogncheck
#endif

namespace headless {

namespace {

// Product name for building the default user agent string.
const char kHeadlessProductName[] = "HeadlessChrome";

#if defined(HEADLESS_USE_PREFS)
const base::FilePath::CharType kLocalStateFilename[] =
    FILE_PATH_LITERAL("Local State");
#endif

}  // namespace

HeadlessBrowser::Options::Options()
    : user_agent(embedder_support::BuildUnifiedPlatformUserAgentFromProduct(
          HeadlessBrowser::GetProductNameAndVersion())) {}

HeadlessBrowser::Options::Options(Options&& options) = default;

HeadlessBrowser::Options::~Options() = default;

HeadlessBrowser::Options& HeadlessBrowser::Options::operator=(
    Options&& options) = default;

bool HeadlessBrowser::Options::DevtoolsServerEnabled() {
  return (devtools_pipe_enabled || devtools_port.has_value());
}

/// static
std::string HeadlessBrowser::GetProductNameAndVersion() {
  return std::string(kHeadlessProductName) + "/" + PRODUCT_VERSION;
}

/// static
blink::UserAgentMetadata HeadlessBrowser::GetUserAgentMetadata() {
  auto metadata = embedder_support::GetUserAgentMetadata();
  // Skip override brand version information if components' API returns a blank
  // UserAgentMetadata.
  if (metadata == blink::UserAgentMetadata()) {
    return metadata;
  }
  std::string significant_version = version_info::GetMajorVersionNumber();

  // Use the major version number as a greasing seed
  int seed = 1;
  bool got_seed = base::StringToInt(significant_version, &seed);
  DCHECK(got_seed);

  // Rengenerate the brand version lists with kHeadlessProductName.
  metadata.brand_version_list = embedder_support::GenerateBrandVersionList(
      seed, kHeadlessProductName, significant_version,
      blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list = embedder_support::GenerateBrandVersionList(
      seed, kHeadlessProductName, metadata.full_version,
      blink::UserAgentBrandVersionType::kFullVersion);
  return metadata;
}

HeadlessBrowserImpl::HeadlessBrowserImpl(
    base::OnceCallback<void(HeadlessBrowser*)> on_start_callback)
    : on_start_callback_(std::move(on_start_callback)),
      platform_delegate_(std::make_unique<HeadlessPlatformDelegate>()) {}

HeadlessBrowserImpl::~HeadlessBrowserImpl() = default;

void HeadlessBrowserImpl::SetOptions(HeadlessBrowser::Options options) {
  options_ = std::move(options);
}

HeadlessBrowserContext::Builder
HeadlessBrowserImpl::CreateBrowserContextBuilder() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return HeadlessBrowserContext::Builder(this);
}

scoped_refptr<base::SingleThreadTaskRunner>
HeadlessBrowserImpl::BrowserMainThread() const {
  return content::GetUIThreadTaskRunner({});
}

void HeadlessBrowserImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  weak_ptr_factory_.InvalidateWeakPtrs();
  // Make sure GetAllBrowserContexts is sane if called after this point.
  auto tmp = std::move(browser_contexts_);
  tmp.clear();
  system_request_context_manager_.reset();
  // We might have posted task during shutdown, let these run
  // before quitting the message loop. See ~HeadlessWebContentsImpl
  // for additional context.
  if (quit_main_message_loop_) {
    BrowserMainThread()->PostTask(FROM_HERE,
                                  std::move(quit_main_message_loop_));
  }
}

void HeadlessBrowserImpl::ShutdownWithExitCode(int exit_code) {
  exit_code_ = exit_code;
  Shutdown();
}

std::vector<HeadlessBrowserContext*>
HeadlessBrowserImpl::GetAllBrowserContexts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<HeadlessBrowserContext*> result;
  result.reserve(browser_contexts_.size());

  for (const auto& browser_context_pair : browser_contexts_) {
    result.push_back(browser_context_pair.second.get());
  }

  return result;
}

HeadlessBrowserContext* HeadlessBrowserImpl::CreateBrowserContext(
    HeadlessBrowserContext::Builder* builder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto browser_context = HeadlessBrowserContextImpl::Create(builder);
  HeadlessBrowserContext* result = browser_context.get();
  browser_contexts_[browser_context->Id()] = std::move(browser_context);

  return result;
}

void HeadlessBrowserImpl::DestroyBrowserContext(
    HeadlessBrowserContextImpl* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int erased = browser_contexts_.erase(browser_context->Id());
  DCHECK(erased);
  if (default_browser_context_ == browser_context)
    SetDefaultBrowserContext(nullptr);
}

void HeadlessBrowserImpl::SetDefaultBrowserContext(
    HeadlessBrowserContext* browser_context) {
  DCHECK(!browser_context ||
         this == HeadlessBrowserContextImpl::From(browser_context)->browser());

  default_browser_context_ = browser_context;

  if (default_browser_context_ && !system_request_context_manager_) {
    system_request_context_manager_ =
        HeadlessRequestContextManager::CreateSystemContext(
            HeadlessBrowserContextImpl::From(browser_context)->options(),
            os_crypt_async());
  }
}

HeadlessBrowserContext* HeadlessBrowserImpl::GetDefaultBrowserContext() {
  return default_browser_context_;
}

base::WeakPtr<HeadlessBrowserImpl> HeadlessBrowserImpl::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_ptr_factory_.GetWeakPtr();
}

HeadlessWebContentsImpl* HeadlessBrowserImpl::GetWebContentsForWindowId(
    const int window_id) {
  for (HeadlessBrowserContext* context : GetAllBrowserContexts()) {
    for (HeadlessWebContents* web_contents : context->GetAllWebContents()) {
      auto* contents = HeadlessWebContentsImpl::From(web_contents);
      if (contents->window_id() == window_id) {
        return contents;
      }
    }
  }
  return nullptr;
}

HeadlessBrowserContext* HeadlessBrowserImpl::GetBrowserContextForId(
    const std::string& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto find_it = browser_contexts_.find(id);
  if (find_it == browser_contexts_.end())
    return nullptr;
  return find_it->second.get();
}

bool HeadlessBrowserImpl::ShouldStartDevToolsServer() {
  if (!options()->DevtoolsServerEnabled()) {
    return false;
  }

#if defined(HEADLESS_USE_POLICY)
  CHECK(local_state_);
  if (!IsRemoteDebuggingAllowed(local_state_.get())) {
    // Follow content/browser/devtools/devtools_http_handler.cc that reports its
    // remote debugging port on stderr for symmetry.
    UNSAFE_TODO(fputs(
        "\nDevTools remote debugging is disallowed by the system admin.\n",
        stderr));
    fflush(stderr);
    return false;
  }
#endif
  return true;
}

void HeadlessBrowserImpl::PreMainMessageLoopRun() {
  CreateOSCryptAsync();

  platform_delegate_->Initialize(options_.value());

  // We don't support the tethering domain on this agent host.
  agent_host_ = content::DevToolsAgentHost::CreateForBrowser(
      nullptr, content::DevToolsAgentHost::CreateServerSocketCallback());

  platform_delegate_->Start();

  std::move(on_start_callback_).Run(this);
}

void HeadlessBrowserImpl::WillRunMainMessageLoop(base::RunLoop& run_loop) {
  quit_main_message_loop_ = run_loop.QuitClosure();
}

void HeadlessBrowserImpl::PostMainMessageLoopRun() {
  os_crypt_async_.reset();
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

void HeadlessBrowserImpl::InitializeWebContents(
    HeadlessWebContentsImpl* web_contents) {
  platform_delegate_->InitializeWebContents(web_contents);
}

void HeadlessBrowserImpl::SetWebContentsBounds(
    HeadlessWebContentsImpl* web_contents,
    const gfx::Rect& bounds) {
  platform_delegate_->SetWebContentsBounds(web_contents, bounds);
}

ui::Compositor* HeadlessBrowserImpl::GetCompositor(
    HeadlessWebContentsImpl* web_contents) {
  return platform_delegate_->GetCompositor(web_contents);
}

#if defined(HEADLESS_USE_POLICY)
policy::PolicyService* HeadlessBrowserImpl::GetPolicyService() {
  return policy_connector_ ? policy_connector_->GetPolicyService() : nullptr;
}
#endif

#if defined(HEADLESS_USE_PREFS)
void HeadlessBrowserImpl::CreatePrefService() {
  CHECK(!local_state_);

  scoped_refptr<PersistentPrefStore> pref_store;
  if (options()->user_data_dir.empty()) {
    pref_store = base::MakeRefCounted<InMemoryPrefStore>();
  } else {
    base::FilePath local_state_file =
        options()->user_data_dir.Append(kLocalStateFilename);
    pref_store = base::MakeRefCounted<JsonPrefStore>(
        local_state_file,
        /*pref_filter=*/nullptr,
        /*file_task_runner=*/
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
        /*read_only=*/true);
    auto result = pref_store->ReadPrefs();
    if (result != JsonPrefStore::PREF_READ_ERROR_NONE &&
        result != JsonPrefStore::PREF_READ_ERROR_NO_FILE) {
      LOG(ERROR) << "Failed to read prefs in '" << local_state_file
                 << "', error: " << result;
      BrowserMainThread()->PostTask(
          FROM_HERE,
          base::BindOnce(&HeadlessBrowserImpl::ShutdownWithExitCode,
                         weak_ptr_factory_.GetWeakPtr(), EXIT_FAILURE));
      return;
    }
  }

  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();
#if BUILDFLAG(IS_WIN)
  OSCrypt::RegisterLocalPrefs(pref_registry.get());
#endif

#if defined(HEADLESS_SUPPORT_FIELD_TRIALS)
  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());
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

PrefService* HeadlessBrowserImpl::GetPrefs() {
  return local_state_.get();
}
#endif  // defined(HEADLESS_USE_PREFS)

void HeadlessBrowserImpl::CreateOSCryptAsync() {
  std::vector<std::pair<size_t, std::unique_ptr<os_crypt_async::KeyProvider>>>
      providers;
#if BUILDFLAG(IS_WIN) && defined(HEADLESS_USE_PREFS)
  if (local_state_) {
    providers.emplace_back(std::make_pair(
        /*precedence=*/10u, std::make_unique<os_crypt_async::DPAPIKeyProvider>(
                                local_state_.get())));
  }
#elif BUILDFLAG(IS_APPLE)
  providers.emplace_back(std::make_pair(
      /*precedence=*/10u,
      std::make_unique<os_crypt_async::KeychainKeyProvider>()));
#elif BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  const auto password_store =
      cmd_line->GetSwitchValueASCII(password_manager::kPasswordStore);
  providers.emplace_back(
      /*precedence=*/10u,
      std::make_unique<os_crypt_async::FreedesktopSecretKeyProvider>(
          password_store, kHeadlessProductName, nullptr));
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  providers.emplace_back(
      /*precedence=*/5u, std::make_unique<os_crypt_async::PosixKeyProvider>());
#endif
  os_crypt_async_ =
      std::make_unique<os_crypt_async::OSCryptAsync>(std::move(providers));
}

}  // namespace headless
