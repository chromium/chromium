// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/runtime/runtime_api.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/one_shot_event.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "base/version.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/events/lazy_event_dispatch_util.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/api/runtime.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "storage/browser/file_system/isolated_context.h"
#include "url/gurl.h"

using content::BrowserContext;

namespace extensions {

namespace runtime = api::runtime;

namespace {

const char kNoBackgroundPageError[] = "You do not have a background page.";
const char kPageLoadError[] = "Background page failed to load.";
const char kFailedToCreateOptionsPage[] = "Could not create an options page.";
const char kInstallId[] = "id";
const char kInstallReason[] = "reason";
const char kInstallReasonChromeUpdate[] = "chrome_update";
const char kInstallReasonUpdate[] = "update";
const char kInstallReasonInstall[] = "install";
const char kInstallReasonSharedModuleUpdate[] = "shared_module_update";
const char kInstallPreviousVersion[] = "previousVersion";
const char kInvalidUrlError[] = "Invalid URL: \"*\".";
const char kPlatformInfoUnavailable[] = "Platform information unavailable.";

const char kUpdatesDisabledError[] = "Autoupdate is not enabled.";

// A preference key storing the url loaded when an extension is uninstalled.
const char kUninstallUrl[] = "uninstall_url";

// The name of the directory to be returned by getPackageDirectoryEntry. This
// particular value does not matter to user code, but is chosen for consistency
// with the equivalent Pepper API.
const char kPackageDirectoryPath[] = "crxfs";

// Preference key for storing the last successful restart due to a call to
// chrome.runtime.restartAfterDelay().
constexpr char kPrefLastRestartAfterDelayTime[] =
    "last_restart_after_delay_time";
// Preference key for storing whether the most recent restart was due to a
// successful call to chrome.runtime.restartAfterDelay().
constexpr char kPrefLastRestartWasDueToDelayedRestartApi[] =
    "last_restart_was_due_to_delayed_restart_api";

// Error and status messages strings for the restartAfterDelay() API.
constexpr char kErrorInvalidArgument[] = "Invalid argument: *.";
constexpr char kErrorOnlyKioskModeAllowed[] =
    "API available only for ChromeOS kiosk mode.";
constexpr char kErrorOnlyFirstExtensionAllowed[] =
    "Not the first extension to call this API.";
constexpr char kErrorInvalidStatus[] = "Invalid restart request status.";
constexpr char kErrorRequestedTooSoon[] =
    "Restart was requested too soon. It was throttled instead.";

constexpr int kMinDurationBetweenSuccessiveRestartsHours = 3;

// This is used for unit tests, so that we can test the restartAfterDelay
// API without a kiosk app.
bool allow_non_kiosk_apps_restart_api_for_test = false;

void DispatchOnStartupEventImpl(
    BrowserContext* browser_context,
    const ExtensionId& extension_id,
    bool first_call,
    std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info) {
  // A NULL ContextInfo from the task callback means the page failed
  // to load. Give up.
  if (!context_info && !first_call) {
    return;
  }

  if (ExtensionsBrowserClient::Get()->IsShuttingDown() ||
      !ExtensionsBrowserClient::Get()->IsValidContext(browser_context)) {
    return;
  }

  // Don't send onStartup events to incognito browser contexts.
  if (browser_context->IsOffTheRecord()) {
    return;
  }

  ExtensionSystem* system = ExtensionSystem::Get(browser_context);
  if (!system) {
    return;
  }

  // If this is a persistent background page, we want to wait for it to load
  // (it might not be ready, since this is startup). But only enqueue once.
  // If it fails to load the first time, don't bother trying again.
  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (extension && BackgroundInfo::HasPersistentBackgroundPage(extension) &&
      first_call) {
    const auto context_id =
        LazyContextId::ForExtension(browser_context, extension);
    LazyContextTaskQueue* task_queue = context_id.GetTaskQueue();
    if (task_queue->ShouldEnqueueTask(browser_context, extension)) {
      task_queue->AddPendingTask(
          context_id, base::BindOnce(&DispatchOnStartupEventImpl,
                                     browser_context, extension_id, false));
      return;
    }
  }

  auto event = std::make_unique<Event>(events::RUNTIME_ON_STARTUP,
                                       runtime::OnStartup::kEventName,
                                       base::Value::List());
  EventRouter::Get(browser_context)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

std::string GetUninstallURL(ExtensionPrefs* prefs,
                            const ExtensionId& extension_id) {
  std::string url_string;
  prefs->ReadPrefAsString(extension_id, kUninstallUrl, &url_string);
  return url_string;
}

// Returns true if the given `context` matches the `filter`.
bool ExtensionContextMatchesFilter(
    const api::runtime::ExtensionContext& context,
    const api::runtime::ContextFilter& filter) {
  if (filter.context_types &&
      !base::Contains(*filter.context_types, context.context_type)) {
    return false;
  }
  if (filter.context_ids &&
      !base::Contains(*filter.context_ids, context.context_id)) {
    return false;
  }
  if (filter.tab_ids && !base::Contains(*filter.tab_ids, context.tab_id)) {
    return false;
  }
  if (filter.window_ids &&
      !base::Contains(*filter.window_ids, context.window_id)) {
    return false;
  }
  if (filter.document_ids &&
      (!context.document_id ||
       !base::Contains(*filter.document_ids, *context.document_id))) {
    return false;
  }
  if (filter.frame_ids &&
      !base::Contains(*filter.frame_ids, context.frame_id)) {
    return false;
  }
  if (filter.document_urls &&
      (!context.document_url ||
       !base::Contains(*filter.document_urls, *context.document_url))) {
    return false;
  }
  if (filter.document_origins &&
      (!context.document_origin ||
       !base::Contains(*filter.document_origins, *context.document_origin))) {
    return false;
  }
  if (filter.incognito && *filter.incognito != context.incognito) {
    return false;
  }

  return true;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<RuntimeAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<RuntimeAPI>* RuntimeAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

constexpr int RuntimeAPI::kFastReloadCount;
constexpr int RuntimeAPI::kUnpackedFastReloadCount;

// static
void RuntimeAPI::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kPrefLastRestartWasDueToDelayedRestartApi,
                                false);
  registry->RegisterDoublePref(kPrefLastRestartAfterDelayTime, 0.0);
}

template <>
void BrowserContextKeyedAPIFactory<RuntimeAPI>::DeclareFactoryDependencies() {
  DependsOn(ProcessManagerFactory::GetInstance());
}

RuntimeAPI::RuntimeAPI(content::BrowserContext* context)
    : browser_context_(context),
      minimum_duration_between_restarts_(
          base::Hours(kMinDurationBetweenSuccessiveRestartsHours)),
      dispatch_chrome_updated_event_(false),
      did_read_delayed_restart_preferences_(false),
      was_last_restart_due_to_delayed_restart_api_(false) {
  // RuntimeAPI is redirected in incognito, so |browser_context_| is never
  // incognito.
  DCHECK(!browser_context_->IsOffTheRecord());

  ExtensionSystem::Get(context)->ready().Post(
      FROM_HERE, base::BindOnce(&RuntimeAPI::OnExtensionsReady,
                                weak_ptr_factory_.GetWeakPtr()));
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));
  process_manager_observation_.Observe(ProcessManager::Get(browser_context_));

  delegate_ = ExtensionsBrowserClient::Get()->CreateRuntimeAPIDelegate(
      browser_context_);

  // Check if registered events are up to date. We can only do this once
  // per browser context, since it updates internal state when called.
  dispatch_chrome_updated_event_ =
      ExtensionsBrowserClient::Get()->DidVersionUpdate(browser_context_);

  EventRouter::Get(browser_context_)
      ->lazy_event_dispatch_util()
      ->AddObserver(this);
}

RuntimeAPI::~RuntimeAPI() = default;

void RuntimeAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                   const Extension* extension) {
  if (!dispatch_chrome_updated_event_) {
    return;
  }

  // Dispatch the onInstalled event with reason "chrome_update".
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RuntimeEventRouter::DispatchOnInstalledEvent,
                                static_cast<void*>(browser_context_),
                                extension->id(), base::Version(), true));
}

void RuntimeAPI::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  RuntimeEventRouter::OnExtensionUninstalled(browser_context_, extension->id(),
                                             reason);
}

void RuntimeAPI::Shutdown() {
  delegate_->RemoveUpdateObserver(this);
  EventRouter::Get(browser_context_)
      ->lazy_event_dispatch_util()
      ->RemoveObserver(this);
}

void RuntimeAPI::OnAppUpdateAvailable(const Extension* extension) {
  RuntimeEventRouter::DispatchOnUpdateAvailableEvent(
      browser_context_, extension->id(), extension->manifest()->value());
}

void RuntimeAPI::OnChromeUpdateAvailable() {
  RuntimeEventRouter::DispatchOnBrowserUpdateAvailableEvent(browser_context_);
}

void RuntimeAPI::OnBackgroundHostStartup(const Extension* extension) {
  RuntimeEventRouter::DispatchOnStartupEvent(browser_context_, extension->id());
}

void RuntimeAPI::ReloadExtension(const ExtensionId& extension_id) {
  delegate_->ReloadExtension(extension_id);
}

bool RuntimeAPI::CheckForUpdates(
    const ExtensionId& extension_id,
    RuntimeAPIDelegate::UpdateCheckCallback callback) {
  return delegate_->CheckForUpdates(extension_id, std::move(callback));
}

void RuntimeAPI::OpenURL(const GURL& update_url) {
  delegate_->OpenURL(update_url);
}

bool RuntimeAPI::GetPlatformInfo(runtime::PlatformInfo* info) {
  return delegate_->GetPlatformInfo(info);
}

bool RuntimeAPI::RestartDevice(std::string* error_message) {
  if (was_last_restart_due_to_delayed_restart_api_ &&
      (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode() ||
       allow_non_kiosk_apps_restart_api_for_test)) {
    // We don't allow an app by calling chrome.runtime.restart() to clear the
    // throttle enforced on it when calling chrome.runtime.restartAfterDelay(),
    // i.e. the app can't unthrottle itself.
    // When running in forced kiosk app mode, we assume the following restart
    // request will succeed.
    PrefService* pref_service =
        ExtensionsBrowserClient::Get()->GetPrefServiceForContext(
            browser_context_);
    DCHECK(pref_service);
    pref_service->SetBoolean(kPrefLastRestartWasDueToDelayedRestartApi, true);
  }
  return delegate_->RestartDevice(error_message);
}

RuntimeAPI::RestartAfterDelayStatus RuntimeAPI::RestartDeviceAfterDelay(
    const ExtensionId& extension_id,
    int seconds_from_now) {
  // To achieve as much accuracy as possible, record the time of the call as
  // |now| here.
  const base::Time now = base::Time::NowFromSystemTime();

  if (schedule_restart_first_extension_id_.empty()) {
    schedule_restart_first_extension_id_ = extension_id;
  } else if (extension_id != schedule_restart_first_extension_id_) {
    // We only allow the first extension to call this API to call it repeatedly.
    // Any other extension will fail.
    return RestartAfterDelayStatus::FAILED_NOT_FIRST_EXTENSION;
  }

  MaybeCancelRunningDelayedRestartTimer();

  if (seconds_from_now == -1) {
    // We already stopped the running timer (if any).
    return RestartAfterDelayStatus::SUCCESS_RESTART_CANCELED;
  }

  if (!did_read_delayed_restart_preferences_) {
    // Try to read any previous successful restart attempt time resulting from
    // this API.
    PrefService* pref_service =
        ExtensionsBrowserClient::Get()->GetPrefServiceForContext(
            browser_context_);
    DCHECK(pref_service);

    was_last_restart_due_to_delayed_restart_api_ =
        pref_service->GetBoolean(kPrefLastRestartWasDueToDelayedRestartApi);
    if (was_last_restart_due_to_delayed_restart_api_) {
      // We clear this bit if the previous restart was due to this API, so that
      // we don't throttle restart requests coming after other restarts or
      // shutdowns not caused by the runtime API.
      pref_service->SetBoolean(kPrefLastRestartWasDueToDelayedRestartApi,
                               false);
    }

    last_delayed_restart_time_ = base::Time::FromSecondsSinceUnixEpoch(
        pref_service->GetDouble(kPrefLastRestartAfterDelayTime));

    if (!allow_non_kiosk_apps_restart_api_for_test) {
      // Don't read every time unless in tests.
      did_read_delayed_restart_preferences_ = true;
    }
  }

  return ScheduleDelayedRestart(now, seconds_from_now);
}

bool RuntimeAPI::OpenOptionsPage(const Extension* extension,
                                 content::BrowserContext* browser_context) {
  return delegate_->OpenOptionsPage(extension, browser_context);
}

void RuntimeAPI::MaybeCancelRunningDelayedRestartTimer() {
  if (restart_after_delay_timer_.IsRunning()) {
    restart_after_delay_timer_.Stop();
  }
}

void RuntimeAPI::OnExtensionsReady() {
  // We're done restarting Chrome after an update.
  dispatch_chrome_updated_event_ = false;
  delegate_->AddUpdateObserver(this);
}

RuntimeAPI::RestartAfterDelayStatus RuntimeAPI::ScheduleDelayedRestart(
    const base::Time& now,
    int seconds_from_now) {
  base::TimeDelta delay_till_restart = base::Seconds(seconds_from_now);

  // Throttle restart requests that are received too soon successively, only if
  // the previous restart was due to this API.
  bool was_throttled = false;
  if (was_last_restart_due_to_delayed_restart_api_) {
    base::Time future_restart_time = now + delay_till_restart;
    base::TimeDelta delta_since_last_restart =
        future_restart_time > last_delayed_restart_time_
            ? future_restart_time - last_delayed_restart_time_
            : base::TimeDelta::Max();
    if (delta_since_last_restart < minimum_duration_between_restarts_) {
      // Schedule the restart after |minimum_duration_between_restarts_| has
      // passed.
      delay_till_restart = minimum_duration_between_restarts_ -
                           (now - last_delayed_restart_time_);
      was_throttled = true;
    }
  }

  restart_after_delay_timer_.Start(
      FROM_HERE, delay_till_restart,
      base::BindOnce(&RuntimeAPI::OnDelayedRestartTimerTimeout,
                     weak_ptr_factory_.GetWeakPtr()));

  return was_throttled ? RestartAfterDelayStatus::FAILED_THROTTLED
                       : RestartAfterDelayStatus::SUCCESS_RESTART_SCHEDULED;
}

void RuntimeAPI::OnDelayedRestartTimerTimeout() {
  // We can persist "now" as the last successful restart time, assuming that the
  // following restart request will succeed, since it can only fail if requested
  // by non kiosk apps, and we prevent that from the beginning (unless in
  // unit tests).
  // This assumption is important, since once restart is requested, we might not
  // have enough time to persist the data to disk.
  double now = base::Time::NowFromSystemTime().InSecondsFSinceUnixEpoch();
  PrefService* pref_service =
      ExtensionsBrowserClient::Get()->GetPrefServiceForContext(
          browser_context_);
  DCHECK(pref_service);
  pref_service->SetDouble(kPrefLastRestartAfterDelayTime, now);
  pref_service->SetBoolean(kPrefLastRestartWasDueToDelayedRestartApi, true);

  std::string error_message;
  const bool success = delegate_->RestartDevice(&error_message);

  // Make sure our above assumption is maintained.
  DCHECK(success || allow_non_kiosk_apps_restart_api_for_test);
}

void RuntimeAPI::AllowNonKioskAppsInRestartAfterDelayForTesting() {
  allow_non_kiosk_apps_restart_api_for_test = true;
}

///////////////////////////////////////////////////////////////////////////////

// static
void RuntimeEventRouter::DispatchOnStartupEvent(
    content::BrowserContext* context,
    const ExtensionId& extension_id) {
  DispatchOnStartupEventImpl(context, extension_id, true, nullptr);
}

// static
void RuntimeEventRouter::DispatchOnInstalledEvent(
    void* context_id,
    const ExtensionId& extension_id,
    const base::Version& old_version,
    bool chrome_updated) {
  if (!ExtensionsBrowserClient::Get()->IsValidContext(context_id)) {
    return;
  }
  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(context_id);
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system) {
    return;
  }

  // Only dispatch runtime.onInstalled events if:
  // 1. the extension has just been installed/updated
  // 2. chrome has updated and the extension had runtime.onInstalled listener.
  // TODO(devlin): Having the chrome_update event tied to onInstalled has caused
  // some issues in the past, see crbug.com/451268. We might want to eventually
  // decouple the chrome_updated event from onInstalled and/or throttle
  // dispatching the chrome_updated event.
  if (chrome_updated && !EventRouter::Get(context)->ExtensionHasEventListener(
                            extension_id, runtime::OnInstalled::kEventName)) {
    return;
  }

  base::Value::List event_args;
  base::Value::Dict info;
  if (old_version.IsValid()) {
    info.Set(kInstallReason, kInstallReasonUpdate);
    info.Set(kInstallPreviousVersion, old_version.GetString());
  } else if (chrome_updated) {
    info.Set(kInstallReason, kInstallReasonChromeUpdate);
  } else {
    info.Set(kInstallReason, kInstallReasonInstall);
  }
  event_args.Append(std::move(info));
  EventRouter* event_router = EventRouter::Get(context);
  DCHECK(event_router);
  auto event = std::make_unique<Event>(events::RUNTIME_ON_INSTALLED,
                                       runtime::OnInstalled::kEventName,
                                       std::move(event_args));
  event_router->DispatchEventWithLazyListener(extension_id, std::move(event));

  if (old_version.IsValid()) {
    const Extension* extension =
        ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
            extension_id);
    if (extension && SharedModuleInfo::IsSharedModule(extension)) {
      std::unique_ptr<ExtensionSet> dependents =
          system->GetDependentExtensions(extension);
      for (ExtensionSet::const_iterator i = dependents->begin();
           i != dependents->end(); i++) {
        base::Value::List sm_event_args;
        base::Value::Dict sm_info;
        sm_info.Set(kInstallReason, kInstallReasonSharedModuleUpdate);
        sm_info.Set(kInstallPreviousVersion, old_version.GetString());
        sm_info.Set(kInstallId, extension_id);
        sm_event_args.Append(std::move(sm_info));
        auto sm_event = std::make_unique<Event>(
            events::RUNTIME_ON_INSTALLED, runtime::OnInstalled::kEventName,
            std::move(sm_event_args));
        event_router->DispatchEventWithLazyListener((*i)->id(),
                                                    std::move(sm_event));
      }
    }
  }
}

// static
void RuntimeEventRouter::DispatchOnUpdateAvailableEvent(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const base::Value::Dict* manifest) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system) {
    return;
  }

  base::Value::List args;
  args.Append(manifest->Clone());
  EventRouter* event_router = EventRouter::Get(context);
  DCHECK(event_router);
  auto event = std::make_unique<Event>(events::RUNTIME_ON_UPDATE_AVAILABLE,
                                       runtime::OnUpdateAvailable::kEventName,
                                       std::move(args));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

// static
void RuntimeEventRouter::DispatchOnBrowserUpdateAvailableEvent(
    content::BrowserContext* context) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system) {
    return;
  }

  EventRouter* event_router = EventRouter::Get(context);
  DCHECK(event_router);
  auto event = std::make_unique<Event>(
      events::RUNTIME_ON_BROWSER_UPDATE_AVAILABLE,
      runtime::OnBrowserUpdateAvailable::kEventName, base::Value::List());
  event_router->BroadcastEvent(std::move(event));
}

// static
void RuntimeEventRouter::DispatchOnRestartRequiredEvent(
    content::BrowserContext* context,
    const std::string& app_id,
    api::runtime::OnRestartRequiredReason reason) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system) {
    return;
  }

  std::unique_ptr<Event> event(
      new Event(events::RUNTIME_ON_RESTART_REQUIRED,
                runtime::OnRestartRequired::kEventName,
                api::runtime::OnRestartRequired::Create(reason)));
  EventRouter* event_router = EventRouter::Get(context);
  DCHECK(event_router);
  event_router->DispatchEventToExtension(app_id, std::move(event));
}

// static
void RuntimeEventRouter::OnExtensionUninstalled(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    UninstallReason reason) {
  if (!(reason == UNINSTALL_REASON_USER_INITIATED ||
        reason == UNINSTALL_REASON_MANAGEMENT_API ||
        reason == UNINSTALL_REASON_CHROME_WEBSTORE)) {
    return;
  }

  GURL uninstall_url(
      GetUninstallURL(ExtensionPrefs::Get(context), extension_id));

  if (!uninstall_url.SchemeIsHTTPOrHTTPS()) {
    // Previous versions of Chrome allowed non-http(s) URLs to be stored in the
    // prefs. Now they're disallowed, but the old data may still exist.
    return;
  }

  // Blocklisted extensions should not open uninstall_url.
  if (blocklist_prefs::IsExtensionBlocklisted(
          extension_id, extensions::ExtensionPrefs::Get(context))) {
    return;
  }

  RuntimeAPI::GetFactoryInstance()->Get(context)->OpenURL(uninstall_url);
}

void RuntimeAPI::OnExtensionInstalledAndLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const base::Version& previous_version) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RuntimeEventRouter::DispatchOnInstalledEvent,
                                static_cast<void*>(browser_context_),
                                extension->id(), previous_version, false));
}

ExtensionFunction::ResponseAction RuntimeGetBackgroundPageFunction::Run() {
  if (!BackgroundInfo::HasBackgroundPage(extension())) {
    return RespondNow(Error(kNoBackgroundPageError));
  }
  ExtensionHost* host = ProcessManager::Get(browser_context())
                            ->GetBackgroundHostForExtension(extension_id());
  const auto context_id =
      LazyContextId::ForExtension(browser_context(), extension());
  LazyContextTaskQueue* task_queue = context_id.GetTaskQueue();
  if (task_queue->ShouldEnqueueTask(browser_context(), extension())) {
    task_queue->AddPendingTask(
        context_id,
        base::BindOnce(&RuntimeGetBackgroundPageFunction::OnPageLoaded, this));
  } else if (host) {
    OnPageLoaded(std::make_unique<LazyContextTaskQueue::ContextInfo>(host));
  } else {
    return RespondNow(Error(kNoBackgroundPageError));
  }

  return RespondLater();
}

void RuntimeGetBackgroundPageFunction::OnPageLoaded(
    std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info) {
  if (context_info) {
    Respond(NoArguments());
  } else {
    Respond(Error(kPageLoadError));
  }
}

ExtensionFunction::ResponseAction RuntimeOpenOptionsPageFunction::Run() {
  RuntimeAPI* api = RuntimeAPI::GetFactoryInstance()->Get(browser_context());
  return RespondNow(api->OpenOptionsPage(extension(), browser_context())
                        ? NoArguments()
                        : Error(kFailedToCreateOptionsPage));
}

ExtensionFunction::ResponseAction RuntimeSetUninstallURLFunction::Run() {
  std::optional<api::runtime::SetUninstallURL::Params> params =
      api::runtime::SetUninstallURL::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!params->url.empty() && !GURL(params->url).SchemeIsHTTPOrHTTPS()) {
    return RespondNow(Error(kInvalidUrlError, params->url));
  }

  ExtensionPrefs::Get(browser_context())
      ->UpdateExtensionPref(extension_id(), kUninstallUrl,
                            base::Value(std::move(params->url)));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction RuntimeReloadFunction::Run() {
  RuntimeAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->ReloadExtension(extension_id());
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction RuntimeRequestUpdateCheckFunction::Run() {
  if (!RuntimeAPI::GetFactoryInstance()
           ->Get(browser_context())
           ->CheckForUpdates(
               extension_id(),
               base::BindOnce(&RuntimeRequestUpdateCheckFunction::CheckComplete,
                              this))) {
    return RespondNow(Error(kUpdatesDisabledError));
  }
  return RespondLater();
}

void RuntimeRequestUpdateCheckFunction::CheckComplete(
    const RuntimeAPIDelegate::UpdateCheckResult& result) {
  api::runtime::RequestUpdateCheck::Results::Result return_result;
  return_result.status = result.status;
  return_result.version = std::optional<std::string>(result.version);
  Respond(WithArguments(return_result.ToValue()));
}

ExtensionFunction::ResponseAction RuntimeRestartFunction::Run() {
  std::string message;
  bool result = RuntimeAPI::GetFactoryInstance()
                    ->Get(browser_context())
                    ->RestartDevice(&message);
  if (!result) {
    return RespondNow(Error(message));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction RuntimeRestartAfterDelayFunction::Run() {
  if (!allow_non_kiosk_apps_restart_api_for_test &&
      !ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode()) {
    return RespondNow(Error(kErrorOnlyKioskModeAllowed));
  }

  std::optional<api::runtime::RestartAfterDelay::Params> params =
      api::runtime::RestartAfterDelay::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  int seconds = params->seconds;

  if (seconds <= 0 && seconds != -1) {
    return RespondNow(
        Error(kErrorInvalidArgument, base::NumberToString(seconds)));
  }

  RuntimeAPI::RestartAfterDelayStatus request_status =
      RuntimeAPI::GetFactoryInstance()
          ->Get(browser_context())
          ->RestartDeviceAfterDelay(extension()->id(), seconds);

  switch (request_status) {
    case RuntimeAPI::RestartAfterDelayStatus::FAILED_NOT_FIRST_EXTENSION:
      return RespondNow(Error(kErrorOnlyFirstExtensionAllowed));

    case RuntimeAPI::RestartAfterDelayStatus::FAILED_THROTTLED:
      return RespondNow(Error(kErrorRequestedTooSoon));

    case RuntimeAPI::RestartAfterDelayStatus::SUCCESS_RESTART_CANCELED:
    case RuntimeAPI::RestartAfterDelayStatus::SUCCESS_RESTART_SCHEDULED:
      return RespondNow(NoArguments());
  }

  NOTREACHED_IN_MIGRATION();
  return RespondNow(Error(kErrorInvalidStatus));
}

ExtensionFunction::ResponseAction RuntimeGetPlatformInfoFunction::Run() {
  runtime::PlatformInfo info;
  if (!RuntimeAPI::GetFactoryInstance()
           ->Get(browser_context())
           ->GetPlatformInfo(&info)) {
    return RespondNow(Error(kPlatformInfoUnavailable));
  }
  return RespondNow(
      ArgumentList(runtime::GetPlatformInfo::Results::Create(info)));
}

ExtensionFunction::ResponseAction
RuntimeGetPackageDirectoryEntryFunction::Run() {
  storage::IsolatedContext* isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  std::string relative_path = kPackageDirectoryPath;
  base::FilePath path = extension_->path();
  storage::IsolatedContext::ScopedFSHandle filesystem =
      isolated_context->RegisterFileSystemForPath(
          storage::kFileSystemTypeLocal, std::string(), path, &relative_path);

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  policy->GrantReadFileSystem(source_process_id(), filesystem.id());
  base::Value::Dict dict;
  dict.Set("fileSystemId", filesystem.id());
  dict.Set("baseName", relative_path);
  return RespondNow(WithArguments(std::move(dict)));
}

RuntimeGetContextsFunction::RuntimeGetContextsFunction() = default;
RuntimeGetContextsFunction::~RuntimeGetContextsFunction() = default;

ExtensionFunction::ResponseAction RuntimeGetContextsFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(extension());

  auto params = api::runtime::GetContexts::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const api::runtime::ContextFilter& filter = params->filter;

  std::vector<api::runtime::ExtensionContext> result;

  // Minor optimization: only construct the context if there's a chance it will
  // match the filter.
  if (!filter.context_types ||
      base::Contains(*filter.context_types,
                     api::runtime::ContextType::kBackground)) {
    if (std::optional<api::runtime::ExtensionContext> worker =
            GetWorkerContext()) {
      result.push_back(std::move(*worker));
    }
  }

  std::vector<api::runtime::ExtensionContext> frame_contexts =
      GetFrameContexts();
  result.insert(result.end(), std::make_move_iterator(frame_contexts.begin()),
                std::make_move_iterator(frame_contexts.end()));

  // Erase any contexts that don't match the specified filter.
  std::erase_if(result,
                [&filter](const api::runtime::ExtensionContext& context) {
                  return !ExtensionContextMatchesFilter(context, filter);
                });

  return RespondNow(
      ArgumentList(api::runtime::GetContexts::Results::Create(result)));
}

std::optional<api::runtime::ExtensionContext>
RuntimeGetContextsFunction::GetWorkerContext() {
  ProcessManager* const process_manager =
      ProcessManager::Get(browser_context());
  DCHECK(process_manager);

  std::vector<WorkerId> active_workers =
      process_manager->GetServiceWorkersForExtension(extension()->id());

  // TODO(crbug.com/40936639):Enable this CHECK once multiple active workers is
  // resolved.
  // CHECK_LE(active_workers.size(), 1u)
  //     << "runtime.getContexts() API call found more than one service worker "
  //        "for extension.";

  if (active_workers.empty()) {
    return std::nullopt;
  }

  api::runtime::ExtensionContext context;
  context.context_type = api::runtime::ContextType::kBackground;
  base::Uuid context_id =
      process_manager->GetContextIdForWorker(active_workers[0]);
  CHECK(context_id.is_valid());
  context.context_id = context_id.AsLowercaseString();
  context.tab_id = extension_misc::kUnknownTabId;
  context.window_id = extension_misc::kUnknownWindowId;
  // TODO(devlin): Add extension_misc::kUnknownFrameId and use it here?
  context.frame_id = -1;
  context.incognito = browser_context()->IsOffTheRecord();
  return context;
}

std::vector<api::runtime::ExtensionContext>
RuntimeGetContextsFunction::GetFrameContexts() {
  ProcessManager* const process_manager =
      ProcessManager::Get(browser_context());
  CHECK(process_manager);

  auto get_context_type = [](content::WebContents* web_contents) {
    mojom::ViewType view_type = GetViewType(web_contents);
    switch (view_type) {
      // These should never be reached for extensions capable of calling this
      // method.
      case mojom::ViewType::kInvalid:
      case mojom::ViewType::kAppWindow:
      case mojom::ViewType::kBackgroundContents:
      case mojom::ViewType::kComponent:
      case mojom::ViewType::kExtensionBackgroundPage:
        DUMP_WILL_BE_NOTREACHED();
        break;

      case mojom::ViewType::kExtensionPopup:
        return api::runtime::ContextType::kPopup;
      case mojom::ViewType::kTabContents:
        return api::runtime::ContextType::kTab;
      case mojom::ViewType::kOffscreenDocument:
        return api::runtime::ContextType::kOffscreenDocument;
      case mojom::ViewType::kExtensionSidePanel:
        return api::runtime::ContextType::kSidePanel;
      case mojom::ViewType::kDeveloperTools:
        return api::runtime::ContextType::kDeveloperTools;

      case mojom::ViewType::kExtensionGuest:
        // Skip these view types for now.
        break;

        // Adding a new mojom::ViewType? Consider whether it should be exposed
        // to extension developers via `chrome.runtime.getContexts()` and, if
        // so, add a new entry in the runtime API.
    }

    return api::runtime::ContextType::kNone;
  };

  ProcessManager::FrameSet frames =
      process_manager->GetRenderFrameHostsForExtension(extension()->id());
  std::vector<api::runtime::ExtensionContext> results;

  for (content::RenderFrameHost* host : frames) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(host);
    CHECK(web_contents);

    auto context_type = get_context_type(web_contents);
    if (context_type == api::runtime::ContextType::kNone) {
      // Skip unsupported contexts.
      continue;
    }

    api::runtime::ExtensionContext context;
    context.context_type = context_type;
    context.context_id =
        ExtensionApiFrameIdMap::GetContextId(host).AsLowercaseString();
    context.tab_id = GetTabId(*web_contents);
    context.frame_id = GetFrameId(*host);
    context.window_id = GetWindowId(*web_contents);
    context.document_id =
        ExtensionApiFrameIdMap::GetDocumentId(host).ToString();
    context.document_url = host->GetLastCommittedURL().spec();
    context.document_origin = host->GetLastCommittedOrigin().Serialize();
    context.incognito = host->GetBrowserContext()->IsOffTheRecord();

    results.push_back(std::move(context));
  }

  return results;
}

int RuntimeGetContextsFunction::GetTabId(content::WebContents& web_contents) {
  mojom::ViewType view_type = extensions::GetViewType(&web_contents);

  if (view_type == extensions::mojom::ViewType::kDeveloperTools) {
    return -1;
  }

  return sessions::SessionTabHelper::IdForTab(&web_contents).id();
}

int RuntimeGetContextsFunction::GetFrameId(content::RenderFrameHost& host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&host);
  mojom::ViewType view_type = extensions::GetViewType(web_contents);

  if (view_type == extensions::mojom::ViewType::kDeveloperTools) {
    return -1;
  }

  return ExtensionApiFrameIdMap::GetFrameId(&host);
}

int RuntimeGetContextsFunction::GetWindowId(
    content::WebContents& web_contents) {
  mojom::ViewType view_type = extensions::GetViewType(&web_contents);

  if (view_type != extensions::mojom::ViewType::kDeveloperTools) {
    return sessions::SessionTabHelper::IdForWindowContainingTab(&web_contents)
        .id();
  }

  // For developer tools, ask the embedder for the window ID.
  std::unique_ptr<RuntimeAPIDelegate> delegate =
      ExtensionsBrowserClient::Get()->CreateRuntimeAPIDelegate(
          browser_context());
  return delegate->GetDeveloperToolsWindowId(&web_contents);
}

}  // namespace extensions
