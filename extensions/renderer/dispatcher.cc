// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/dispatcher.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/incognito.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/constants.h"
#include "extensions/common/cors_util.h"
#include "extensions/common/crash_keys.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/common/utils/extension_utils.h"
#include "extensions/grit/extensions_renderer_resources.h"
#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"
#include "extensions/renderer/content_watcher.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_interaction_provider.h"
#include "extensions/renderer/extensions_renderer_api_provider.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/isolated_world_manager.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/script_injection_manager.h"
#include "extensions/renderer/service_worker_data.h"
#include "extensions/renderer/shared_l10n_map.h"
#include "extensions/renderer/static_v8_external_one_byte_string_resource.h"
#include "extensions/renderer/trace_util.h"
#include "extensions/renderer/v8_helpers.h"
#include "extensions/renderer/worker_script_context_set.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "extensions/renderer/worker_thread_util.h"
#include "gin/converter.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "third_party/blink/public/web/web_custom_element.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_controller.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_v8_features.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

using blink::WebDocument;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebView;
using content::RenderThread;

namespace extensions {

namespace {

static const char kOnSuspendEvent[] = "runtime.onSuspend";
static const char kOnSuspendCanceledEvent[] = "runtime.onSuspendCanceled";

enum class ExtensionRendererLoadStatus {
  // Extension is neither loaded in the registry nor unloaded.
  kUnknownExtension = 0,
  // Extension is loaded in the registry.
  kExtensionLoaded = 1,
  // Extension was loaded in the registry, but was unloaded.
  kExtensionUnloaded = 2,
  kMaxValue = kExtensionUnloaded,
};

// Returns whether or not extension APIs are allowed for the specified
// `script_url` and `scope`. The script must be specified in the extension's
// manifest background section and the scope must be the root scope of the
// extension.
bool ExtensionAPIEnabledForServiceWorkerScript(const GURL& scope,
                                               const GURL& script_url) {
  if (!script_url.SchemeIs(kExtensionScheme)) {
    return false;
  }

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(script_url);

  if (!extension || !BackgroundInfo::IsServiceWorkerBased(extension)) {
    return false;
  }

  if (scope != extension->url()) {
    return false;
  }

  const std::string& sw_script =
      BackgroundInfo::GetBackgroundServiceWorkerScript(extension);

  return extension->GetResourceURL(sw_script) == script_url;
}

// Calls a method |method_name| in a module |module_name| belonging to the
// module system from |context|. Intended as a callback target from
// ScriptContextSet::ForEach.
void CallModuleMethod(const std::string& module_name,
                      const std::string& method_name,
                      const base::Value::List* args,
                      ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Context::Scope context_scope(context->v8_context());

  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();

  v8::LocalVector<v8::Value> arguments(context->isolate());
  for (const auto& arg : *args) {
    arguments.push_back(converter->ToV8Value(arg, context->v8_context()));
  }

  context->module_system()->CallModuleMethodSafe(
      module_name, method_name, &arguments);
}

class HandleScopeHelper {
 public:
  HandleScopeHelper(ScriptContext* script_context)
      : handle_scope_(script_context->isolate()),
        context_scope_(script_context->v8_context()) {}

  HandleScopeHelper(const HandleScopeHelper&) = delete;
  HandleScopeHelper& operator=(const HandleScopeHelper&) = delete;

 private:
  v8::HandleScope handle_scope_;
  v8::Context::Scope context_scope_;
};

base::LazyInstance<WorkerScriptContextSet>::DestructorAtExit
    g_worker_script_context_set = LAZY_INSTANCE_INITIALIZER;

// Creates a new extension from the data in the mojom::ExtensionLoadedParams
// object. A context_id needs to be passed because each browser context can have
// different values for default_policy_blocked/allowed_hosts.
// (see extension_util.cc#GetBrowserContextId)
scoped_refptr<Extension> ConvertToExtension(
    mojom::ExtensionLoadedParamsPtr params,
    int context_id,
    std::string* error) {
  // We pass in the |id| to the create call because it will save work in the
  // normal case, and because in tests, extensions may not have paths or keys,
  // but it's important to retain the same id.
  scoped_refptr<Extension> extension =
      Extension::Create(params->path, params->location, params->manifest,
                        params->creation_flags, params->id, error);

  if (!extension.get())
    return extension;

  const PermissionsData* permissions_data = extension->permissions_data();
  permissions_data->SetPermissions(
      std::make_unique<const PermissionSet>(
          std::move(params->active_permissions)),
      std::make_unique<const PermissionSet>(
          std::move(params->withheld_permissions)));
  permissions_data->SetContextId(context_id);

  if (params->uses_default_policy_blocked_allowed_hosts) {
    permissions_data->SetUsesDefaultHostRestrictions();
  } else {
    permissions_data->SetPolicyHostRestrictions(params->policy_blocked_hosts,
                                                params->policy_allowed_hosts);
  }

  for (const auto& pair : params->tab_specific_permissions) {
    permissions_data->UpdateTabSpecificPermissions(pair.first, pair.second);
  }

  extension->SetGUID(params->guid);

  return extension;
}

using IncognitoManifestKeys = api::incognito::ManifestKeys;

base::debug::CrashKeyString* GetCrashKey(const char* key) {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      key, base::debug::CrashKeySize::Size32);
  return crash_key;
}

const ExtensionId& GetExtensionIdValue(const Extension& extension) {
  return extension.id();
}

std::string GetManifestVersionValue(const Extension& extension) {
  return base::NumberToString(extension.manifest_version());
}

const char* GetServiceWorkerBasedValue(const Extension& extension) {
  return BackgroundInfo::IsServiceWorkerBased(&extension) ? "yes" : "no";
}

const char* GetIncognitoModeValue(const Extension& extension) {
  IncognitoInfo* info = static_cast<IncognitoInfo*>(
      extension.GetManifestData(IncognitoManifestKeys::kIncognito));
  if (!info) {
    return "no_incognito_info";
  }
  return api::incognito::ToString(info->mode);
}

const char* GetIncognitoProcessValue(
    const ExtensionsRendererClient* renderer_client) {
  if (!renderer_client) {
    return "no_renderer_client";
  }
  return renderer_client->IsIncognitoProcess() ? "yes" : "no";
}

}  // namespace

namespace debug {

// Helper for adding a set of missing activation token related crash keys.
//
// It is created when being notified that an extension worker will evaluate (is
// in the process of starting) and we might detect that there isn't an
// activation token recorded for the extension worker.
//
// All keys are logged every time this class is instantiated.
class ScopedActivationTokenMissingCrashKeys {
 public:
  explicit ScopedActivationTokenMissingCrashKeys(
      const Extension& extension,
      const ExtensionsRendererClient* renderer_client)
      : extension_id_crash_key_(GetCrashKey("ext_token_id"),
                                GetExtensionIdValue(extension)),
        manifest_version_crash_key_(GetCrashKey("ext_token_manifest_version"),
                                    GetManifestVersionValue(extension)),
        sw_based_crash_key_(GetCrashKey("ext_token_sw_based"),
                            GetServiceWorkerBasedValue(extension)),
        incognito_mode_crash_key_(GetCrashKey("ext_token_incog_mode"),
                                  GetIncognitoModeValue(extension)),
        incognito_process_crash_key_(
            GetCrashKey("ext_token_incog_process"),
            GetIncognitoProcessValue(renderer_client)) {}
  ~ScopedActivationTokenMissingCrashKeys() = default;

 private:
  // ExtensionId of the extension.
  base::debug::ScopedCrashKeyString extension_id_crash_key_;

  // The manifest version of the extension.
  base::debug::ScopedCrashKeyString manifest_version_crash_key_;

  // Whether the extension has a service worker background script registered in
  // the manifest.
  base::debug::ScopedCrashKeyString sw_based_crash_key_;

  // What the api::incognito::IncognitoMode is for the extension.
  base::debug::ScopedCrashKeyString incognito_mode_crash_key_;

  // Whether the renderer process for the extension was launched incognito.
  base::debug::ScopedCrashKeyString incognito_process_crash_key_;
};

}  // namespace debug

Dispatcher::PendingServiceWorker::PendingServiceWorker(
    blink::WebServiceWorkerContextProxy* context_proxy)
    : task_runner(base::SingleThreadTaskRunner::GetCurrentDefault()),
      context_proxy(context_proxy) {
  DCHECK(context_proxy);
}

Dispatcher::PendingServiceWorker::~PendingServiceWorker() = default;

// Note that we can't use Blink public APIs in the constructor because Blink
// is not initialized at the point we create Dispatcher.
Dispatcher::Dispatcher(
    std::vector<std::unique_ptr<const ExtensionsRendererAPIProvider>>
        api_providers)
    : api_providers_(std::move(api_providers)),
      content_watcher_(new ContentWatcher()),
      source_map_(&ui::ResourceBundle::GetSharedInstance()),
      v8_schema_registry_(new V8SchemaRegistry),
      activity_logging_enabled_(false),
      receiver_(this),
      dispatcher_(this) {
  bindings_system_ = CreateBindingsSystem(
      this, IPCMessageSender::CreateMainThreadIPCMessageSender());

  script_context_set_ =
      std::make_unique<ScriptContextSet>(&active_extension_ids_);
  user_script_set_manager_ = std::make_unique<UserScriptSetManager>();
  script_injection_manager_ =
      std::make_unique<ScriptInjectionManager>(user_script_set_manager_.get());
  user_script_set_manager_observation_.Observe(user_script_set_manager_.get());
  PopulateSourceMap();
  // Ideally this should be done after checking
  // ExtensionAPIEnabledInExtensionServiceWorkers(), but the Dispatcher is
  // created so early that sending an IPC from browser/ process to synchronize
  // this enabled-ness is too late.
  WorkerThreadDispatcher::Get()->Init(RenderThread::Get());

  // Register WebSecurityPolicy allowlists for the chrome-extension:// scheme.
  WebString extension_scheme(WebString::FromASCII(kExtensionScheme));

  // Extension resources are HTTP-like and safe to expose to the fetch API. The
  // rules for the fetch API are consistent with XHR.
  WebSecurityPolicy::RegisterURLSchemeAsSupportingFetchAPI(extension_scheme);

  // Register WebSecurityPolicy allowlists for the file:// scheme.
  WebString file_scheme(WebString::FromASCII(url::kFileScheme));

  // Extensions are allowed to make cross-origin requests to file scheme iff the
  // user explicitly grants them access post-installation in the
  // chrome://extensions page.
  WebSecurityPolicy::RegisterURLSchemeAsSupportingFetchAPI(file_scheme);

  // Extension resources, when loaded as the top-level document, should bypass
  // Blink's strict first-party origin checks.
  WebSecurityPolicy::RegisterURLSchemeAsFirstPartyWhenTopLevel(
      extension_scheme);

  // Disallow running javascript URLs on the chrome-extension scheme.
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      extension_scheme);

  if (base::FeatureList::IsEnabled(
          extensions_features::kAllowSharedArrayBuffersUnconditionally)) {
    WebSecurityPolicy::RegisterURLSchemeAsAllowingSharedArrayBuffers(
        extension_scheme);
  }

  // chrome-extension: resources should be allowed to register ServiceWorkers.
  WebSecurityPolicy::RegisterURLSchemeAsAllowingServiceWorkers(
      extension_scheme);

  WebSecurityPolicy::RegisterURLSchemeAsAllowingWasmEvalCSP(extension_scheme);

  // Initialize host permissions for any extensions that were activated before
  // WebKit was initialized.
  for (const ExtensionId& extension_id : active_extension_ids_) {
    const Extension* extension =
        RendererExtensionRegistry::Get()->GetByID(extension_id);
    CHECK(extension);
    InitOriginPermissions(extension);
  }

  EnableCustomElementAllowlist();
}

Dispatcher::~Dispatcher() {
}

// static
WorkerScriptContextSet* Dispatcher::GetWorkerScriptContextSet() {
  return &(g_worker_script_context_set.Get());
}

// static
bool Dispatcher::ShouldNotifyServiceWorkerOnWebSocketActivity(
    v8::Local<v8::Context> v8_context) {
  ScriptContext* script_context =
      GetWorkerScriptContextSet()->GetContextByV8Context(v8_context);
  // Only notify on web socket activity if the service worker is the background
  // service worker for an extension.
  return script_context &&
         ExtensionAPIEnabledForServiceWorkerScript(
             script_context->service_worker_scope(), script_context->url());
}

void Dispatcher::OnRenderThreadStarted(content::RenderThread* thread) {
  blink::WebScriptController::RegisterExtension(
      SafeBuiltins::CreateV8Extension());
}

void Dispatcher::OnRenderFrameCreated(content::RenderFrame* render_frame) {
  script_injection_manager_->OnRenderFrameCreated(render_frame);
  content_watcher_->OnRenderFrameCreated(render_frame);

  // The RenderFrame comes with the initial empty document already created.
  DidCreateDocumentElement(render_frame->GetWebFrame());
  // We run scripts on the empty document.
  RunScriptsAtDocumentStart(render_frame);
}

bool Dispatcher::IsExtensionActive(const ExtensionId& extension_id) const {
  const bool is_active = base::Contains(active_extension_ids_, extension_id);
  if (is_active)
    CHECK(RendererExtensionRegistry::Get()->Contains(extension_id));
  return is_active;
}

void Dispatcher::DidCreateScriptContext(
    blink::WebLocalFrame* frame,
    const v8::Local<v8::Context>& v8_context,
    int32_t world_id) {
  const base::TimeTicks start_time = base::TimeTicks::Now();

  ScriptContext* context = script_context_set_->Register(
      frame, v8_context, world_id,
      /*is_webview=*/webview_partition_id_.has_value());

  // Initialize origin permissions for content scripts, which can't be
  // initialized in |ActivateExtension|.
  if (context->context_type() == mojom::ContextType::kContentScript) {
    InitOriginPermissions(context->extension());
  }

  context->SetModuleSystem(
      std::make_unique<ModuleSystem>(context, &source_map_));

  ModuleSystem* module_system = context->module_system();

  // Enable natives in startup.
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system);

  RegisterNativeHandlers(module_system, context, bindings_system_.get(),
                         v8_schema_registry_.get());

  bindings_system_->DidCreateScriptContext(context);

  // Inject custom JS into the platform app context.
  if (IsWithinPlatformApp()) {
    module_system->Require("platformApp");
  }

  RequireGuestViewModules(context);

  const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
  switch (context->context_type()) {
    case mojom::ContextType::kUnspecified:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_Unspecified",
                          elapsed);
      break;
    case mojom::ContextType::kPrivilegedExtension:
      // For service workers this is handled in
      // WillEvaluateServiceWorkerOnWorkerThread().
      DCHECK(!context->IsForServiceWorker());
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_Blessed", elapsed);
      break;
    case mojom::ContextType::kUnprivilegedExtension:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_Unblessed",
                          elapsed);
      break;
    case mojom::ContextType::kContentScript:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_ContentScript",
                          elapsed);
      break;
    case mojom::ContextType::kWebPage:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_WebPage", elapsed);
      break;
    case mojom::ContextType::kPrivilegedWebPage:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_BlessedWebPage",
                          elapsed);
      break;
    case mojom::ContextType::kWebUi:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_WebUI", elapsed);
      break;
    case mojom::ContextType::kUntrustedWebUi:
      // Extension APIs in untrusted WebUIs are temporary so don't bother
      // recording metrics for them.
      break;
    case mojom::ContextType::kLockscreenExtension:
      UMA_HISTOGRAM_TIMES(
          "Extensions.DidCreateScriptContext_LockScreenExtension", elapsed);
      break;
    case mojom::ContextType::kOffscreenExtension:
    case mojom::ContextType::kUserScript:
      // We don't really care about offscreen extension context or user script
      // context initialization time at the moment. Offscreen extension context
      // initialization is a strict subset (and very similar to) privileged
      // extension context time, while user script context initialization is
      // very similar to content script initialization.
      break;
  }

  VLOG(1) << "Num tracked contexts: " << script_context_set_->size();

  ExtensionFrameHelper* frame_helper =
      ExtensionFrameHelper::Get(content::RenderFrame::FromWebFrame(frame));
  if (!frame_helper)
    return;  // The frame is invisible to extensions.

  frame_helper->NotifyDidCreateScriptContext(world_id);
}

void Dispatcher::DidInitializeServiceWorkerContextOnWorkerThread(
    blink::WebServiceWorkerContextProxy* context_proxy,
    const GURL& service_worker_scope,
    const GURL& script_url) {
  if (!script_url.SchemeIs(kExtensionScheme))
    return;

  {
    base::AutoLock lock(service_workers_paused_for_on_loaded_message_lock_);
    ExtensionId extension_id =
        RendererExtensionRegistry::Get()->GetExtensionOrAppIDByURL(script_url);
    // If the extension is already loaded we don't have to suspend the service
    // worker. The service worker will continue in
    // Dispatcher::WillEvaluateServiceWorkerOnWorkerThread().
    if (RendererExtensionRegistry::Get()->GetByID(extension_id))
      return;

    // Suspend the service worker until loaded message of the extension comes.
    // The service worker will be resumed in Dispatcher::OnLoaded().
    context_proxy->PauseEvaluation();
    service_workers_paused_for_on_loaded_message_.emplace(
        extension_id, std::make_unique<PendingServiceWorker>(context_proxy));
  }
}

void Dispatcher::WillEvaluateServiceWorkerOnWorkerThread(
    blink::WebServiceWorkerContextProxy* context_proxy,
    v8::Local<v8::Context> v8_context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url,
    const blink::ServiceWorkerToken& service_worker_token) {
  const base::TimeTicks start_time = base::TimeTicks::Now();

  // TODO(crbug.com/40626913): We may want to give service workers not
  // registered by extensions minimal bindings, the same as other webpage-like
  // contexts.
  if (!script_url.SchemeIs(kExtensionScheme)) {
    // Early-out if this isn't a chrome-extension:// scheme, because looking up
    // the extension registry is unnecessary if it's not. Checking this will
    // also skip over hosted apps, which is the desired behavior - hosted app
    // service workers are not our concern.
    return;
  }

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(script_url);

  ExtensionRendererLoadStatus load_status =
      ExtensionRendererLoadStatus::kUnknownExtension;
  if (extension) {
    load_status = ExtensionRendererLoadStatus::kExtensionLoaded;
  } else if (base::Contains(unloaded_extensions_, script_url.host())) {
    // script_url.host() is the extension's ID.
    load_status = ExtensionRendererLoadStatus::kExtensionUnloaded;
  }
  base::UmaHistogramEnumeration(
      "Extensions.ServiceWorkerRenderer."
      "ExtensionLoadStatusInWorkerScriptEvaluation",
      load_status);

  if (!extension) {
    // TODO(kalman): This is no good. Instead we need to either:
    //
    // - Hold onto the v8::Context and create the ScriptContext and install
    //   our bindings when this extension is loaded.
    // - Deal with there being an extension ID (script_url.host()) but no
    //   extension associated with it.
    //
    // The former is safer, but is unfriendly to caching (e.g. session restore).
    // It seems to contradict the service worker idiom.
    //
    // The latter is friendly to caching, but running extension code without an
    // installed extension makes me nervous, and means that we won't be able to
    // expose arbitrary (i.e. capability-checked) extension APIs to service
    // workers. We will probably need to relax some assertions - we just need
    // to find them.
    //
    // Perhaps this could be solved with our own event on the service worker
    // saying that an extension is ready, and documenting that extension APIs
    // won't work before that event has fired?
    return;
  }

  if (!ExtensionAPIEnabledForServiceWorkerScript(service_worker_scope,
                                                 script_url)) {
    return;
  }

  const int thread_id = content::WorkerThread::GetCurrentId();
  CHECK_NE(thread_id, kMainThreadId);

  // Only the script specific in the manifest's background data gets bindings.
  //
  // TODO(crbug.com/40626913): We may want to give other service workers
  // registered by extensions minimal bindings, just as we might want to give
  // them to service workers that aren't registered by extensions.
  ScriptContext* context = new ScriptContext(
      v8_context, nullptr, GenerateHostIdFromExtensionId(extension->id()),
      extension, /*blink_isolated_world_id=*/std::nullopt,
      mojom::ContextType::kPrivilegedExtension, extension,
      mojom::ContextType::kPrivilegedExtension);
  context->set_url(script_url);
  context->set_service_worker_scope(service_worker_scope);
  context->set_service_worker_version_id(service_worker_version_id);

  WorkerThreadDispatcher* worker_dispatcher = WorkerThreadDispatcher::Get();
  std::optional<base::UnguessableToken> worker_activation_token =
      RendererExtensionRegistry::Get()->GetWorkerActivationToken(
          extension->id());

  std::unique_ptr<IPCMessageSender> ipc_sender =
      IPCMessageSender::CreateWorkerThreadIPCMessageSender(
          worker_dispatcher, context_proxy, service_worker_version_id);
  {
    CHECK(extension);
    // TODO(crbug.com/357889496): Remove these crash keys once bug is resolved.
    debug::ScopedActivationTokenMissingCrashKeys activation_token_missing_keys(
        *extension, ExtensionsRendererClient::Get());
    CHECK(worker_activation_token.has_value());
  }
  worker_dispatcher->AddWorkerData(
      context_proxy, service_worker_version_id, worker_activation_token,
      service_worker_token, context,
      CreateBindingsSystem(worker_dispatcher, std::move(ipc_sender)));

  // TODO(lazyboy): Make sure accessing |source_map_| in worker thread is
  // safe.
  context->SetModuleSystem(
      std::make_unique<ModuleSystem>(context, &source_map_));

  ModuleSystem* module_system = context->module_system();
  // Enable natives in startup.
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system);
  NativeExtensionBindingsSystem* worker_bindings_system =
      WorkerThreadDispatcher::GetBindingsSystem();
  RegisterNativeHandlers(module_system, context, worker_bindings_system,
                         WorkerThreadDispatcher::GetV8SchemaRegistry());

  worker_bindings_system->DidCreateScriptContext(context);

  // TODO(lazyboy): Get rid of RequireGuestViewModules() as this doesn't seem
  // necessary for Extension SW.
  RequireGuestViewModules(context);

  WorkerThreadDispatcher::GetServiceWorkerData()->Init();
  g_worker_script_context_set.Get().Insert(base::WrapUnique(context));

  const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES(
      "Extensions.DidInitializeServiceWorkerContextOnWorkerThread2", elapsed);
}

void Dispatcher::WillReleaseScriptContext(
    blink::WebLocalFrame* frame,
    const v8::Local<v8::Context>& v8_context,
    int32_t world_id) {
  ScriptContext* context = script_context_set_->GetByV8Context(v8_context);
  if (!context)
    return;
  bindings_system_->WillReleaseScriptContext(context);

  script_context_set_->Remove(context);
  VLOG(1) << "Num tracked contexts: " << script_context_set_->size();
}

void Dispatcher::DidStartServiceWorkerContextOnWorkerThread(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
  if (!ExtensionAPIEnabledForServiceWorkerScript(service_worker_scope,
                                                 script_url)) {
    return;
  }

  const int thread_id = content::WorkerThread::GetCurrentId();
  CHECK_NE(thread_id, kMainThreadId);
  auto* service_worker_data = WorkerThreadDispatcher::GetServiceWorkerData();
  service_worker_data->GetServiceWorkerHost()->DidStartServiceWorkerContext(
      service_worker_data->context()->GetExtensionID(),
      *service_worker_data->activation_sequence(), service_worker_scope,
      service_worker_version_id, thread_id);
}

void Dispatcher::WillDestroyServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> v8_context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
  // Note that using ExtensionAPIEnabledForServiceWorkerScript() won't work here
  // as RendererExtensionRegistry might have already unloaded this extension.
  // Use the existence of ServiceWorkerData as the source of truth instead.
  if (auto* service_worker_data =
          WorkerThreadDispatcher::GetServiceWorkerData()) {
    const int thread_id = content::WorkerThread::GetCurrentId();
    CHECK_NE(thread_id, kMainThreadId);

    // TODO(lazyboy/devlin): Should this cleanup happen in a worker class, like
    // WorkerThreadDispatcher? If so, we should move the initialization as well.
    ScriptContext* script_context = service_worker_data->context();
    NativeExtensionBindingsSystem* worker_bindings_system =
        service_worker_data->bindings_system();
    if (worker_bindings_system) {
      worker_bindings_system->WillReleaseScriptContext(script_context);
      service_worker_data->GetServiceWorkerHost()->DidStopServiceWorkerContext(
          script_context->GetExtensionID(),
          *service_worker_data->activation_sequence(), service_worker_scope,
          service_worker_version_id, thread_id);
    }
    // Note: we have to remove the context (and thus perform invalidation on
    // the native handlers) prior to removing the worker data, which destroys
    // the associated bindings system.
    g_worker_script_context_set.Get().Remove(v8_context, script_url);
    WorkerThreadDispatcher::Get()->RemoveWorkerData(service_worker_version_id);
  } else {
    // If extension APIs in service workers aren't enabled, we just need to
    // remove the context.
    g_worker_script_context_set.Get().Remove(v8_context, script_url);
  }

  ExtensionId extension_id =
      RendererExtensionRegistry::Get()->GetExtensionOrAppIDByURL(script_url);
  {
    base::AutoLock lock(service_workers_paused_for_on_loaded_message_lock_);
    service_workers_paused_for_on_loaded_message_.erase(extension_id);
  }
}

void Dispatcher::DidCreateDocumentElement(blink::WebLocalFrame* frame) {
  // Note: use GetEffectiveDocumentURLForContext() and not just
  // frame->document()->url() so that this also injects the stylesheet on
  // about:blank frames that are hosted in the extension process. (Even though
  // this is used to determine whether to inject a stylesheet, we don't use
  // GetEffectiveDocumentURLForInjection() because we inject based on whether
  // it is an extension context, rather than based on the extension's injection
  // permissions.)
  GURL effective_document_url =
      ScriptContext::GetEffectiveDocumentURLForContext(
          frame, frame->GetDocument().Url(), true /* match_about_blank */);

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          effective_document_url);

  if (extension &&
      (extension->is_extension() || extension->is_platform_app())) {
    int resource_id = extension->is_platform_app() ? IDR_PLATFORM_APP_CSS
                                                   : IDR_EXTENSION_FONTS_CSS;
    std::string stylesheet =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            resource_id);
    base::ReplaceFirstSubstringAfterOffset(
        &stylesheet, 0, "$FONTFAMILY", system_font_family_);
    base::ReplaceFirstSubstringAfterOffset(
        &stylesheet, 0, "$FONTSIZE", system_font_size_);

    // Blink doesn't let us define an additional user agent stylesheet, so
    // we insert the default platform app or extension stylesheet into all
    // documents that are loaded in each app or extension.
    frame->GetDocument().InsertStyleSheet(WebString::FromUTF8(stylesheet));
  }

  // If this is an extension options page, and the extension has opted into
  // using Chrome styles, then insert the Chrome extension stylesheet.
  if (extension && extension->is_extension() &&
      OptionsPageInfo::ShouldUseChromeStyle(extension) &&
      effective_document_url == OptionsPageInfo::GetOptionsPage(extension)) {
    std::string extension_css =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_EXTENSION_CSS);
    frame->GetDocument().InsertStyleSheet(WebString::FromUTF8(extension_css));
  }
}

void Dispatcher::RunScriptsAtDocumentStart(content::RenderFrame* render_frame) {
  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  if (!frame_helper)
    return;  // The frame is invisible to extensions.

  frame_helper->RunScriptsAtDocumentStart();
  // |frame_helper| and |render_frame| might be dead by now.
}

void Dispatcher::RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) {
  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  if (!frame_helper)
    return;  // The frame is invisible to extensions.

  frame_helper->RunScriptsAtDocumentEnd();
  // |frame_helper| and |render_frame| might be dead by now.
}

void Dispatcher::RunScriptsAtDocumentIdle(content::RenderFrame* render_frame) {
  ExtensionFrameHelper* frame_helper = ExtensionFrameHelper::Get(render_frame);
  if (!frame_helper)
    return;  // The frame is invisible to extensions.

  frame_helper->RunScriptsAtDocumentIdle();
  // |frame_helper| and |render_frame| might be dead by now.
}

void Dispatcher::DispatchEventHelper(
    const mojom::HostID& host_id,
    const std::string& event_name,
    const base::Value::List& event_args,
    mojom::EventFilteringInfoPtr filtering_info) const {
  script_context_set_->ForEach(
      host_id, nullptr,
      base::BindRepeating(
          &NativeExtensionBindingsSystem::DispatchEventInContext,
          base::Unretained(bindings_system_.get()), event_name,
          std::cref(event_args), base::OwnedRef(std::move(filtering_info))));
}

void Dispatcher::InvokeModuleSystemMethod(content::RenderFrame* render_frame,
                                          const ExtensionId& extension_id,
                                          const std::string& module_name,
                                          const std::string& function_name,
                                          const base::Value::List& args) {
  script_context_set_->ForEach(
      GenerateHostIdFromExtensionId(extension_id), render_frame,
      base::BindRepeating(&CallModuleMethod, module_name, function_name,
                          &args));
}

void Dispatcher::ExecuteDeclarativeScript(content::RenderFrame* render_frame,
                                          int tab_id,
                                          const ExtensionId& extension_id,
                                          const std::string& script_id,
                                          const GURL& url) {
  TRACE_RENDERER_EXTENSION_EVENT("Dispatcher::ExecuteDeclarativeScript",
                                 extension_id);
  script_injection_manager_->ExecuteDeclarativeScript(
      render_frame, tab_id, extension_id, script_id, url);
}

void Dispatcher::ExecuteCode(mojom::ExecuteCodeParamsPtr param,
                             mojom::LocalFrame::ExecuteCodeCallback callback,
                             content::RenderFrame* render_frame) {
  TRACE_RENDERER_EXTENSION_EVENT("Dispatcher::ExecuteCode", param->host_id->id);
  script_injection_manager_->HandleExecuteCode(
      std::move(param), std::move(callback), render_frame);
}

void Dispatcher::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  // This base::Unretained() is safe, because:
  // 1) the Dispatcher is a RenderThreadObserver and outlives the RenderThread.
  // 2) |asscoiated_interfaces| is owned by the RenderThread.
  // As well the Dispatcher is owned by the
  // ExtensionsRendererClient, which in turn is a leaky LazyInstance (and thus
  // never deleted).
  associated_interfaces->AddInterface<mojom::Renderer>(base::BindRepeating(
      &Dispatcher::OnRendererAssociatedRequest, base::Unretained(this)));
  associated_interfaces->AddInterface<mojom::EventDispatcher>(
      base::BindRepeating(&Dispatcher::OnEventDispatcherRequest,
                          base::Unretained(this)));
}

void Dispatcher::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(mojom::Renderer::Name_);
  associated_interfaces->RemoveInterface(mojom::EventDispatcher::Name_);
}

void Dispatcher::OnRendererAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::Renderer> receiver) {
  receiver_.Bind(std::move(receiver));
}

void Dispatcher::OnEventDispatcherRequest(
    mojo::PendingAssociatedReceiver<mojom::EventDispatcher> dispatcher) {
  CHECK(!dispatcher_.is_bound());
  dispatcher_.Bind(std::move(dispatcher));
}

void Dispatcher::ActivateExtension(const ExtensionId& extension_id) {
  TRACE_RENDERER_EXTENSION_EVENT("Dispatcher::ActivateExtension", extension_id);

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension) {
    // Extension was activated but was never loaded. This probably means that
    // the renderer failed to load it (or the browser failed to tell us when it
    // did). Failures shouldn't happen, but instead of crashing there (which
    // executes on all renderers) just log an error and dump without crashing.
    std::string& error = extension_load_errors_[extension_id];
    char minidump[256];
    base::debug::Alias(&minidump);
    base::snprintf(minidump, std::size(minidump), "e::dispatcher:%s:%s",
                   extension_id.c_str(), error.c_str());
    LOG(ERROR) << extension_id << " was never loaded: " << error;
    base::debug::DumpWithoutCrashing();
    return;
  }

  // It's possible that the same extension might generate multiple activation
  // messages, for example from an extension background page followed by an
  // extension subframe on a regular tab.  Ensure that any given extension is
  // only activated once.
  if (IsExtensionActive(extension_id))
    return;

  active_extension_ids_.insert(extension_id);

  if (activity_logging_enabled_) {
    DOMActivityLogger::AttachToWorld(DOMActivityLogger::kMainWorldId,
                                     extension_id);
  }

  InitOriginPermissions(extension);

  UpdateActiveExtensions();
}

void Dispatcher::LoadExtensions(
    std::vector<mojom::ExtensionLoadedParamsPtr> loaded_extensions) {
  for (auto& param : loaded_extensions) {
    std::string error;
    ExtensionId id = param->id;
    std::optional<base::UnguessableToken> worker_activation_token =
        param->worker_activation_token;

    scoped_refptr<const Extension> extension =
        ConvertToExtension(std::move(param), kRendererProfileId, &error);
    if (!extension.get()) {
      NOTREACHED_IN_MIGRATION() << error;
      // Note: in tests |param.id| has been observed to be empty (see comment
      // just below) so this isn't all that reliable.
      extension_load_errors_[id] = error;
      continue;
    }

    RendererExtensionRegistry* extension_registry =
        RendererExtensionRegistry::Get();

    // The order of setting the token before inserting the extension is
    // intentional so that DidInitializeServiceWorkerContextOnWorkerThread()
    // will pause the worker evaluation
    // (WillEvaluateServiceWorkerOnWorkerThread()) from running before we have
    // these two necessary pieces of information for setting up the extension
    // bindings.
    if (worker_activation_token.has_value()) {
      extension_registry->SetWorkerActivationToken(
          extension, std::move(*worker_activation_token));
    }
    // TODO(jlulejian): This is deliberately a CHECK because other parts of the
    // renderer assume that an extension has been loaded (e.g. specifically
    // worker script evaluation process). If this fails, other CHECK()s and
    // logic will fail too.
    CHECK(extension_registry->Insert(extension));

    unloaded_extensions_.erase(extension->id());

    ExtensionsRendererClient::Get()->OnExtensionLoaded(*extension);

    // Resume service worker if it is suspended.
    {
      base::AutoLock lock(service_workers_paused_for_on_loaded_message_lock_);
      auto it =
          service_workers_paused_for_on_loaded_message_.find(extension->id());
      if (it != service_workers_paused_for_on_loaded_message_.end()) {
        scoped_refptr<base::SingleThreadTaskRunner> task_runner =
            std::move(it->second->task_runner);
        // Using base::Unretained() should be fine as this won't get destructed.
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&Dispatcher::ResumeEvaluationOnWorkerThread,
                           base::Unretained(this), extension->id()));
      }
    }
  }

  // Update the available bindings for all contexts. These may have changed if
  // an externally_connectable extension was loaded that can connect to an
  // open webpage.
  UpdateAllBindings(/*api_permissions_changed=*/false);
}

void Dispatcher::UnloadExtension(const ExtensionId& extension_id) {
  TRACE_RENDERER_EXTENSION_EVENT("Dispatcher::UnloadExtension", extension_id);

  // An extension should be in the registry if we are unloading it. Otherwise we
  // might be doing something out of the expected order.
  CHECK(RendererExtensionRegistry::Get()->Remove(extension_id));

  unloaded_extensions_.insert(extension_id);

  ExtensionsRendererClient::Get()->OnExtensionUnloaded(extension_id);

  bindings_system_->OnExtensionRemoved(extension_id);

  active_extension_ids_.erase(extension_id);

  user_script_set_manager_->OnExtensionUnloaded(extension_id);

  script_injection_manager_->OnExtensionUnloaded(extension_id);

  // If the extension is later reloaded with a different set of permissions,
  // we'd like it to get a new isolated world ID, so that it can pick up the
  // changed origin allowlist.
  IsolatedWorldManager::GetInstance().RemoveIsolatedWorlds(extension_id);

  // Inform the bindings system that the contexts will be removed to allow time
  // to clear out context-specific data, and then remove the contexts
  // themselves.
  script_context_set_->ForEach(
      GenerateHostIdFromExtensionId(extension_id), nullptr,
      base::BindRepeating(
          &NativeExtensionBindingsSystem::WillReleaseScriptContext,
          base::Unretained(bindings_system_.get())));
  script_context_set_->OnExtensionUnloaded(extension_id);

  // Update the available bindings for the remaining contexts. These may have
  // changed if an externally_connectable extension is unloaded and a webpage
  // is no longer accessible.
  UpdateAllBindings(/*api_permissions_changed=*/false);

  // Invalidates the messages map for the extension in case the extension is
  // reloaded with a new messages map.
  SharedL10nMap::GetInstance().EraseMessagesMap(extension_id);

  // Update the origin access map so that any content scripts injected no longer
  // have dedicated allow/block lists for extra origins.
  WebSecurityPolicy::ClearOriginAccessListForOrigin(
      Extension::GetBaseURLFromExtensionId(extension_id));

  // We don't do anything with existing platform-app stylesheets. They will
  // stay resident, but the URL pattern corresponding to the unloaded
  // extension's URL just won't match anything anymore.
}

void Dispatcher::SuspendExtension(
    const ExtensionId& extension_id,
    mojom::Renderer::SuspendExtensionCallback callback) {
  TRACE_RENDERER_EXTENSION_EVENT("Dispatcher::SuspendExtension", extension_id);

  // Dispatch the suspend event. This doesn't go through the standard event
  // dispatch machinery because it requires special handling. We need to let
  // the browser know when we are starting and stopping the event dispatch, so
  // that it still considers the extension idle despite any activity the suspend
  // event creates.
  DispatchEventHelper(GenerateHostIdFromExtensionId(extension_id),
                      kOnSuspendEvent, base::Value::List(), nullptr);
  std::move(callback).Run();
}

void Dispatcher::CancelSuspendExtension(const ExtensionId& extension_id) {
  DispatchEventHelper(GenerateHostIdFromExtensionId(extension_id),
                      kOnSuspendCanceledEvent, base::Value::List(), nullptr);
}

void Dispatcher::SetSystemFont(const std::string& font_family,
                               const std::string& font_size) {
  system_font_family_ = font_family;
  system_font_size_ = font_size;
}

void Dispatcher::SetWebViewPartitionID(const std::string& partition_id) {
  // |webview_partition_id_| cannot be changed once set.
  CHECK(!webview_partition_id_ || webview_partition_id_ == partition_id);
  webview_partition_id_ = partition_id;
}

void Dispatcher::SetScriptingAllowlist(
    const std::vector<ExtensionId>& extension_ids) {
  ExtensionsClient::Get()->SetScriptingAllowlist(extension_ids);
}

void Dispatcher::UpdateDefaultPolicyHostRestrictions(
    URLPatternSet default_policy_blocked_hosts,
    URLPatternSet default_policy_allowed_hosts) {
  PermissionsData::SetDefaultPolicyHostRestrictions(
      kRendererProfileId, default_policy_blocked_hosts,
      default_policy_allowed_hosts);
  // Update blink host permission allowlist exceptions for all loaded
  // extensions.
  for (const ExtensionId& extension_id :
       RendererExtensionRegistry::Get()->GetIDs()) {
    const Extension* extension =
        RendererExtensionRegistry::Get()->GetByID(extension_id);
    if (extension->permissions_data()->UsesDefaultPolicyHostRestrictions()) {
      UpdateOriginPermissions(*extension);
    }
  }
  UpdateAllBindings(/*api_permissions_changed=*/false);
}

void Dispatcher::UpdateUserScriptWorlds(
    std::vector<mojom::UserScriptWorldInfoPtr> infos) {
  for (const auto& info : infos) {
    IsolatedWorldManager::GetInstance().SetUserScriptWorldProperties(
        info->extension_id, info->world_id, info->csp, info->enable_messaging);
  }
}

void Dispatcher::ClearUserScriptWorldConfig(
    const ExtensionId& extension_id,
    const std::optional<std::string>& world_id) {
  IsolatedWorldManager::GetInstance().ClearUserScriptWorldProperties(
      extension_id, world_id);
}

void Dispatcher::UpdateUserHostRestrictions(URLPatternSet user_blocked_hosts,
                                            URLPatternSet user_allowed_hosts) {
  PermissionsData::SetUserHostRestrictions(kRendererProfileId,
                                           std::move(user_blocked_hosts),
                                           std::move(user_allowed_hosts));

  // TODO(crbug.com/40803363): Update origin permissions and bindings as
  // we do with policy host restrictions above.  Currently, user host
  // restrictions aren't used in the origin access allowlist, so there's no
  // point in updating it.
}

void Dispatcher::UpdateTabSpecificPermissions(const ExtensionId& extension_id,
                                              URLPatternSet new_hosts,
                                              int tab_id,
                                              bool update_origin_allowlist) {
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension)
    return;

  extension->permissions_data()->UpdateTabSpecificPermissions(
      tab_id, PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                            new_hosts.Clone(), new_hosts.Clone()));

  if (update_origin_allowlist)
    UpdateOriginPermissions(*extension);
}

void Dispatcher::UpdateUserScripts(
    base::ReadOnlySharedMemoryRegion shared_memory,
    mojom::HostIDPtr host_id) {
  TRACE_RENDERER_EXTENSION_EVENT("Dispatcher::UpdateUserScripts", host_id->id);
  user_script_set_manager_->OnUpdateUserScripts(std::move(shared_memory),
                                                *host_id);
}

void Dispatcher::ClearTabSpecificPermissions(
    const std::vector<ExtensionId>& extension_ids,
    int tab_id,
    bool update_origin_allowlist) {
  for (const ExtensionId& id : extension_ids) {
    const Extension* extension = RendererExtensionRegistry::Get()->GetByID(id);
    if (extension) {
      extension->permissions_data()->ClearTabSpecificPermissions(tab_id);
      if (update_origin_allowlist)
        UpdateOriginPermissions(*extension);
    }
  }
}

void Dispatcher::WatchPages(const std::vector<std::string>& css_selectors) {
  DCHECK(content_watcher_);
  content_watcher_->OnWatchPages(css_selectors);
}

void Dispatcher::DispatchEvent(mojom::DispatchEventParamsPtr params,
                               base::Value::List event_args,
                               DispatchEventCallback callback) {
  CHECK_EQ(params->worker_thread_id, kMainThreadId);
  CHECK(params->host_id);
  content::RenderFrame* background_frame = nullptr;
  if (params->host_id->type == mojom::HostID::HostType::kExtensions) {
    background_frame = ExtensionFrameHelper::GetBackgroundPageFrame(
        GenerateExtensionIdFromHostId(*params->host_id));
  }
  ScriptContext* background_context = nullptr;
  if (background_frame) {
    background_context =
        ScriptContextSet::GetMainWorldContextForFrame(background_frame);
  }
  bool event_has_listener_in_background_context =
      background_context && bindings_system_->HasEventListenerInContext(
                                params->event_name, background_context);

  // Synthesize a user gesture if this was in response to user action; this is
  // necessary if the gesture was e.g. by clicking on the extension toolbar
  // icon, context menu entry, etc.
  //
  // This will only add an active user gesture for the background page, so any
  // listeners in different frames (like a popup or tab) won't be able to use
  // the user gesture. This is intentional, since frames other than the
  // background page should have their own user gestures, such as through button
  // clicks.
  if (params->is_user_gesture && background_context &&
      event_has_listener_in_background_context) {
    background_frame->GetWebFrame()->NotifyUserActivation(
        blink::mojom::UserActivationNotificationType::kExtensionEvent);
  }

  DispatchEventHelper(*params->host_id, params->event_name, event_args,
                      std::move(params->filtering_info));
  std::move(callback).Run(event_has_listener_in_background_context);
}

void Dispatcher::SetDeveloperMode(bool current_developer_mode) {
  SetCurrentDeveloperMode(kRendererProfileId, current_developer_mode);
  // Since this affects the availability of different APIs, we indicate that
  // api permissions may have changed.
  UpdateAllBindings(/*api_permissions_changed=*/true);
}

void Dispatcher::SetSessionInfo(version_info::Channel channel,
                                mojom::FeatureSessionType session_type,
                                bool is_lock_screen_context) {
  SetCurrentChannel(channel);
  SetCurrentFeatureSessionType(session_type);
  script_context_set_->set_is_lock_screen_context(is_lock_screen_context);
}

void Dispatcher::ShouldSuspend(ShouldSuspendCallback callback) {
  std::move(callback).Run();
}

void Dispatcher::TransferBlobs(TransferBlobsCallback callback) {
  std::move(callback).Run();
}

void Dispatcher::UpdatePermissions(const ExtensionId& extension_id,
                                   PermissionSet active_permissions,
                                   PermissionSet withheld_permissions,
                                   URLPatternSet policy_blocked_hosts,
                                   URLPatternSet policy_allowed_hosts,
                                   bool uses_default_policy_host_restrictions) {
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension)
    return;

  if (uses_default_policy_host_restrictions) {
    extension->permissions_data()->SetUsesDefaultHostRestrictions();
  } else {
    extension->permissions_data()->SetPolicyHostRestrictions(
        policy_blocked_hosts, policy_allowed_hosts);
  }

  extension->permissions_data()->SetPermissions(
      std::make_unique<const PermissionSet>(std::move(active_permissions)),
      std::make_unique<const PermissionSet>(std::move(withheld_permissions)));

  UpdateOriginPermissions(*extension);

  UpdateBindingsForExtension(*extension);
}

void Dispatcher::SetActivityLoggingEnabled(bool enabled) {
  activity_logging_enabled_ = enabled;
  if (enabled) {
    for (const ExtensionId& id : active_extension_ids_) {
      DOMActivityLogger::AttachToWorld(DOMActivityLogger::kMainWorldId, id);
    }
  }
  script_injection_manager_->set_activity_logging_enabled(enabled);
  user_script_set_manager_->set_activity_logging_enabled(enabled);
}

void Dispatcher::OnUserScriptsUpdated(const mojom::HostID& changed_host) {
  // Update the set of active extensions if `changed_host` is an extension and
  // it has scripts.
  if (changed_host.type == mojom::HostID::HostType::kExtensions)
    UpdateActiveExtensions();
}

ScriptContextSetIterable* Dispatcher::GetScriptContextSet() {
  return script_context_set_iterator();
}

void Dispatcher::UpdateActiveExtensions() {
  std::set<ExtensionId> active_extensions = active_extension_ids_;
  user_script_set_manager_->GetAllActiveExtensionIds(&active_extensions);

  // In single-process mode, the browser process reports the active extensions.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kSingleProcess)) {
    crash_keys::SetActiveExtensions(active_extensions);
  }
}

void Dispatcher::InitOriginPermissions(const Extension* extension) {
  UpdateOriginPermissions(*extension);
}

void Dispatcher::UpdateOriginPermissions(const Extension& extension) {
  // Remove all old patterns associated with this extension.
  WebSecurityPolicy::ClearOriginAccessListForOrigin(extension.url());

  std::vector<network::mojom::CorsOriginPatternPtr> allow_list =
      CreateCorsOriginAccessAllowList(extension);
  ExtensionsClient::Get()->AddOriginAccessPermissions(
      extension, IsExtensionActive(extension.id()), &allow_list);
  for (const auto& entry : allow_list) {
    WebSecurityPolicy::AddOriginAccessAllowListEntry(
        extension.url(), WebString::FromUTF8(entry->protocol),
        WebString::FromUTF8(entry->domain), entry->port,
        entry->domain_match_mode, entry->port_match_mode, entry->priority);
  }

  for (const auto& entry : CreateCorsOriginAccessBlockList(extension)) {
    WebSecurityPolicy::AddOriginAccessBlockListEntry(
        extension.url(), WebString::FromUTF8(entry->protocol),
        WebString::FromUTF8(entry->domain), entry->port,
        entry->domain_match_mode, entry->port_match_mode, entry->priority);
  }
}

void Dispatcher::EnableCustomElementAllowlist() {
  blink::WebCustomElement::AddEmbedderCustomElementName("appview");
  blink::WebCustomElement::AddEmbedderCustomElementName("extensionoptions");
  blink::WebCustomElement::AddEmbedderCustomElementName("webview");
  for (const auto& api_provider : api_providers_) {
    api_provider->EnableCustomElementAllowlist();
  }
}

void Dispatcher::UpdateAllBindings(bool api_permissions_changed) {
  bindings_system_->UpdateBindings(ExtensionId() /* all contexts */,
                                   api_permissions_changed,
                                   script_context_set_iterator());

  WorkerThreadDispatcher::Get()->UpdateAllServiceWorkerBindings();
}

void Dispatcher::UpdateBindingsForExtension(const Extension& extension) {
  bindings_system_->UpdateBindings(extension.id(),
                                   true /* permissions_changed */,
                                   script_context_set_iterator());

  // Update Service Worker bindings too, if applicable.
  if (!BackgroundInfo::IsServiceWorkerBased(&extension))
    return;

  const bool updated =
      WorkerThreadDispatcher::Get()->UpdateBindingsForWorkers(extension.id());
  // TODO(lazyboy): When can this fail?
  DCHECK(updated) << "Some or all workers failed to update bindings.";
}

// NOTE: please use the naming convention "foo_natives" for these.
void Dispatcher::RegisterNativeHandlers(
    ModuleSystem* module_system,
    ScriptContext* context,
    NativeExtensionBindingsSystem* bindings_system,
    V8SchemaRegistry* v8_schema_registry) {
  for (const auto& api_provider : api_providers_) {
    api_provider->RegisterNativeHandlers(module_system, bindings_system,
                                         v8_schema_registry, context);
  }
}

void Dispatcher::PopulateSourceMap() {
  for (const auto& api_provider : api_providers_) {
    api_provider->PopulateSourceMap(&source_map_);
  }
}

bool Dispatcher::IsWithinPlatformApp() {
  for (auto iter = active_extension_ids_.begin();
       iter != active_extension_ids_.end(); ++iter) {
    const Extension* extension =
        RendererExtensionRegistry::Get()->GetByID(*iter);
    if (extension && extension->is_platform_app())
      return true;
  }
  return false;
}

void Dispatcher::RequireGuestViewModules(ScriptContext* context) {
  ModuleSystem* module_system = context->module_system();
  bool requires_guest_view_module = false;

  // This determines whether to register error-providing custom elements for the
  // GuestView types that are not available. We only do this in contexts where
  // it is possible to gain access to a given GuestView element by declaring the
  // necessary permission in a manifest file. We don't want to define
  // error-providing elements in other extension contexts as the names could
  // collide with names used in the extension. Also, WebUIs may be allowlisted
  // to use GuestViews, but we don't define the error-providing elements in this
  // case.
  const bool is_platform_app =
      context->context_type() == mojom::ContextType::kPrivilegedExtension &&
      !context->IsForServiceWorker() && context->extension() &&
      context->extension()->is_platform_app();
  const bool app_view_permission_exists = is_platform_app;
  // The webview permission is also available to internal allowlisted
  // extensions, but not to extensions in general.
  const bool web_view_permission_exists = is_platform_app;

  // TODO(fsamuel): Eagerly calling Require on context startup is expensive.
  // It would be better if there were a light way of detecting when a webview
  // or appview is created and only then set up the infrastructure.

  // Require AppView.
  if (context->GetAvailability("appViewEmbedderInternal").is_available()) {
    requires_guest_view_module = true;
    module_system->Require("appViewElement");
  } else if (app_view_permission_exists) {
    module_system->Require("appViewDeny");
  }

  // Require ExtensionOptions.
  if (context->GetAvailability("extensionOptionsInternal").is_available()) {
    requires_guest_view_module = true;
    module_system->Require("extensionOptionsElement");
  }

  // Require WebView.
  if (context->GetAvailability("webViewInternal").is_available()) {
    requires_guest_view_module = true;

    for (const auto& api_provider : api_providers_) {
      api_provider->RequireWebViewModules(context);
    }
  } else if (web_view_permission_exists) {
    module_system->Require("webViewDeny");
  }

  if (requires_guest_view_module) {
    // If a frame has guest view custom elements defined, we need to make sure
    // the custom elements are also defined in subframes. The subframes will
    // need a scripting context which we will need to forcefully create if
    // the subframe doesn't otherwise have any scripts.
    context->web_frame()
        ->View()
        ->GetSettings()
        ->SetForceMainWorldInitialization(true);
  }
}

std::unique_ptr<NativeExtensionBindingsSystem> Dispatcher::CreateBindingsSystem(
    NativeExtensionBindingsSystem::Delegate* delegate,
    std::unique_ptr<IPCMessageSender> ipc_sender) {
  auto bindings_system = std::make_unique<NativeExtensionBindingsSystem>(
      delegate, std::move(ipc_sender));
  for (const auto& api_provider : api_providers_) {
    api_provider->AddBindingsSystemHooks(this, bindings_system.get());
  }
  return bindings_system;
}

void Dispatcher::ResumeEvaluationOnWorkerThread(
    const ExtensionId& extension_id) {
  base::AutoLock lock(service_workers_paused_for_on_loaded_message_lock_);
  auto it = service_workers_paused_for_on_loaded_message_.find(extension_id);
  if (it != service_workers_paused_for_on_loaded_message_.end()) {
    blink::WebServiceWorkerContextProxy* context_proxy =
        it->second->context_proxy;
    context_proxy->ResumeEvaluation();
    service_workers_paused_for_on_loaded_message_.erase(it);
  }
}

}  // namespace extensions
