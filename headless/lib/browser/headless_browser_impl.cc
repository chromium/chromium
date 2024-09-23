// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/user_agent.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

#if BUILDFLAG(IS_WIN)
#include "base/command_line.h"
#include "headless/public/switches.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
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
constexpr gfx::Size kDefaultWindowSize(800, 600);

constexpr gfx::FontRenderParams::Hinting kDefaultFontRenderHinting =
    gfx::FontRenderParams::Hinting::HINTING_FULL;

#if defined(HEADLESS_USE_PREFS)
const base::FilePath::CharType kLocalStateFilename[] =
    FILE_PATH_LITERAL("Local State");
#endif

}  // namespace

using Options = HeadlessBrowser::Options;
using Builder = HeadlessBrowser::Options::Builder;

Options::Options()
    : user_agent(content::BuildUserAgentFromProduct(
          HeadlessBrowser::GetProductNameAndVersion())),
      window_size(kDefaultWindowSize),
      font_render_hinting(kDefaultFontRenderHinting) {}

Options::Options(Options&& options) = default;

Options::~Options() = default;

Options& Options::operator=(Options&& options) = default;

bool Options::DevtoolsServerEnabled() {
  return (devtools_pipe_enabled || devtools_port.has_value());
}

Builder::Builder() = default;

Builder::~Builder() = default;

Builder& Builder::SetUserAgent(const std::string& agent) {
  options_.user_agent = agent;
  return *this;
}

Builder& Builder::SetEnableLazyLoading(bool enable) {
  options_.lazy_load_enabled = enable;
  return *this;
}

Builder& Builder::SetAcceptLanguage(const std::string& language) {
  options_.accept_language = language;
  return *this;
}

Builder& Builder::SetEnableBeginFrameControl(bool enable) {
  options_.enable_begin_frame_control = enable;
  return *this;
}

Builder& Builder::EnableDevToolsServer(int port) {
  options_.devtools_port = port;
  return *this;
}

Builder& Builder::EnableDevToolsPipe() {
  options_.devtools_pipe_enabled = true;
  return *this;
}

Builder& Builder::SetProxyConfig(std::unique_ptr<net::ProxyConfig> config) {
  options_.proxy_config = std::move(config);
  return *this;
}

Builder& Builder::SetUserDataDir(const base::FilePath& dir) {
  options_.user_data_dir = dir;
  return *this;
}

Builder& Builder::SetDiskCacheDir(const base::FilePath& dir) {
  options_.disk_cache_dir = dir;
  return *this;
}

Builder& Builder::SetWindowSize(const gfx::Size& size) {
  options_.window_size = size;
  return *this;
}

Builder& Builder::SetIncognitoMode(bool incognito) {
  options_.incognito_mode = incognito;
  return *this;
}

Builder& Builder::SetBlockNewWebContents(bool block) {
  options_.block_new_web_contents = block;
  return *this;
}

Builder& Builder::SetFontRenderHinting(gfx::FontRenderParams::Hinting hinting) {
  options_.font_render_hinting = hinting;
  return *this;
}

Builder& Builder::SetForceNewBrowsingInstance(bool force) {
  options_.force_new_browsing_instance = force;
  return *this;
}

Options Builder::Build() {
  return std::move(options_);
}

/// static
std::string HeadlessBrowser::GetProductNameAndVersion() {
  return std::string(kHeadlessProductName) + "/" + PRODUCT_VERSION;
}

/// static
blink::UserAgentMetadata HeadlessBrowser::GetUserAgentMetadata() {
  auto metadata = embedder_support::GetUserAgentMetadata(nullptr);
  // Skip override brand version information if components' API returns a blank
  // UserAgentMetadata.
  if (metadata == blink::UserAgentMetadata()) {
    return metadata;
  }
  std::string significant_version = version_info::GetMajorVersionNumber();
  constexpr bool kEnableUpdatedGreaseByPolicy = true;

  // Use the major version number as a greasing seed
  int seed = 1;
  bool got_seed = base::StringToInt(significant_version, &seed);
  DCHECK(got_seed);

  // Rengenerate the brand version lists with kHeadlessProductName.
  metadata.brand_version_list = embedder_support::GenerateBrandVersionList(
      seed, kHeadlessProductName, significant_version, std::nullopt,
      std::nullopt, kEnableUpdatedGreaseByPolicy,
      blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list = embedder_support::GenerateBrandVersionList(
      seed, kHeadlessProductName, metadata.full_version, std::nullopt,
      std::nullopt, kEnableUpdatedGreaseByPolicy,
      blink::UserAgentBrandVersionType::kFullVersion);
  return metadata;
}

HeadlessBrowserImpl::HeadlessBrowserImpl(
    base::OnceCallback<void(HeadlessBrowser*)> on_start_callback)
    : on_start_callback_(std::move(on_start_callback)) {}

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
  if (system_request_context_manager_) {
    content::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, system_request_context_manager_.release());
  }
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
            HeadlessBrowserContextImpl::From(browser_context)->options());
  }
}

HeadlessBrowserContext* HeadlessBrowserImpl::GetDefaultBrowserContext() {
  return default_browser_context_;
}

base::WeakPtr<HeadlessBrowserImpl> HeadlessBrowserImpl::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_ptr_factory_.GetWeakPtr();
}

HeadlessWebContents* HeadlessBrowserImpl::GetWebContentsForDevToolsAgentHostId(
    const std::string& devtools_agent_host_id) {
  for (HeadlessBrowserContext* context : GetAllBrowserContexts()) {
    HeadlessWebContents* web_contents =
        context->GetWebContentsForDevToolsAgentHostId(devtools_agent_host_id);
    if (web_contents)
      return web_contents;
  }
  return nullptr;
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
    fputs("\nDevTools remote debugging is disallowed by the system admin.\n",
          stderr);
    fflush(stderr);
    return false;
  }
#endif
  return true;
}

void HeadlessBrowserImpl::PreMainMessageLoopRun() {
  PlatformInitialize();

  // We don't support the tethering domain on this agent host.
  agent_host_ = content::DevToolsAgentHost::CreateForBrowser(
      nullptr, content::DevToolsAgentHost::CreateServerSocketCallback());

  PlatformStart();
  std::move(on_start_callback_).Run(this);
}

void HeadlessBrowserImpl::WillRunMainMessageLoop(base::RunLoop& run_loop) {
  quit_main_message_loop_ = run_loop.QuitClosure();
}

void HeadlessBrowserImpl::PostMainMessageLoopRun() {
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

}  // namespace headless
