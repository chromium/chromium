// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/dispatcher.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/feature_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/content_capabilities_handler.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/common/view_type.h"
#include "extensions/grit/extensions_renderer_resources.h"
#include "extensions/renderer/api_activity_logger.h"
#include "extensions/renderer/api_definitions_natives.h"
#include "extensions/renderer/app_window_custom_bindings.h"
#include "extensions/renderer/blob_native_handler.h"
#include "extensions/renderer/content_watcher.h"
#include "extensions/renderer/context_menus_custom_bindings.h"
#include "extensions/renderer/dispatcher_delegate.h"
#include "extensions/renderer/display_source_custom_bindings.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/event_bindings.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/file_system_natives.h"
#include "extensions/renderer/guest_view/guest_view_internal_custom_bindings.h"
#include "extensions/renderer/id_generator_custom_bindings.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/js_extension_bindings_system.h"
#include "extensions/renderer/logging_native_handler.h"
#include "extensions/renderer/messaging_bindings.h"
#include "extensions/renderer/messaging_util.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/process_info_native_handler.h"
#include "extensions/renderer/render_frame_observer_natives.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/renderer_messaging_service.h"
#include "extensions/renderer/request_sender.h"
#include "extensions/renderer/runtime_custom_bindings.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/script_injection_manager.h"
#include "extensions/renderer/send_request_natives.h"
#include "extensions/renderer/set_icon_natives.h"
#include "extensions/renderer/static_v8_external_one_byte_string_resource.h"
#include "extensions/renderer/test_features_native_handler.h"
#include "extensions/renderer/test_native_handler.h"
#include "extensions/renderer/user_gestures_native_handler.h"
#include "extensions/renderer/utils_native_handler.h"
#include "extensions/renderer/v8_context_native_handler.h"
#include "extensions/renderer/v8_helpers.h"
#include "extensions/renderer/wake_event_page.h"
#include "extensions/renderer/worker_script_context_set.h"
#include "extensions/renderer/worker_thread_dispatcher.h"
#include "gin/converter.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_custom_element.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_scoped_user_gesture.h"
#include "third_party/blink/public/web/web_script_controller.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

using blink::WebDocument;
using blink::WebScopedUserGesture;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebView;
using content::RenderThread;

namespace extensions {

namespace {

static const char kOnSuspendEvent[] = "runtime.onSuspend";
static const char kOnSuspendCanceledEvent[] = "runtime.onSuspendCanceled";

void CrashOnException(const v8::TryCatch& trycatch) {
  NOTREACHED();
}

// Calls a method |method_name| in a module |module_name| belonging to the
// module system from |context|. Intended as a callback target from
// ScriptContextSet::ForEach.
void CallModuleMethod(const std::string& module_name,
                      const std::string& method_name,
                      const base::ListValue* args,
                      ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Context::Scope context_scope(context->v8_context());

  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();

  std::vector<v8::Local<v8::Value>> arguments;
  for (const auto& arg : *args) {
    arguments.push_back(converter->ToV8Value(&arg, context->v8_context()));
  }

  context->module_system()->CallModuleMethodSafe(
      module_name, method_name, &arguments);
}

// This handles the "chrome." root API object in script contexts.
class ChromeNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit ChromeNativeHandler(ScriptContext* context)
      : ObjectBackedNativeHandler(context) {}

  // ObjectBackedNativeHandler:
  void AddRoutes() override {
    RouteHandlerFunction(
        "GetChrome",
        base::Bind(&ChromeNativeHandler::GetChrome, base::Unretained(this)));
  }

  void GetChrome(const v8::FunctionCallbackInfo<v8::Value>& args) {
    // Check for the chrome property. If one doesn't exist, create one.
    v8::Local<v8::String> chrome_string(
        v8::String::NewFromUtf8(context()->isolate(), "chrome",
                                v8::NewStringType::kInternalized)
            .ToLocalChecked());
    v8::Local<v8::Object> global(context()->v8_context()->Global());
    v8::Local<v8::Value> chrome(global->Get(chrome_string));
    if (chrome->IsUndefined()) {
      chrome = v8::Object::New(context()->isolate());
      global->Set(context()->v8_context(), chrome_string, chrome).ToChecked();
    }
    args.GetReturnValue().Set(chrome);
  }
};

base::LazyInstance<WorkerScriptContextSet>::DestructorAtExit
    g_worker_script_context_set = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Note that we can't use Blink public APIs in the constructor becase Blink
// is not initialized at the point we create Dispatcher.
Dispatcher::Dispatcher(std::unique_ptr<DispatcherDelegate> delegate)
    : delegate_(std::move(delegate)),
      content_watcher_(new ContentWatcher()),
      source_map_(&ui::ResourceBundle::GetSharedInstance()),
      v8_schema_registry_(new V8SchemaRegistry),
      user_script_set_manager_observer_(this),
      activity_logging_enabled_(false) {
  bindings_system_ = CreateBindingsSystem(
      IPCMessageSender::CreateMainThreadIPCMessageSender());

  script_context_set_.reset(new ScriptContextSet(&active_extension_ids_));
  user_script_set_manager_.reset(new UserScriptSetManager());
  script_injection_manager_.reset(
      new ScriptInjectionManager(user_script_set_manager_.get()));
  user_script_set_manager_observer_.Add(user_script_set_manager_.get());
  PopulateSourceMap();
  WakeEventPage::Get()->Init(RenderThread::Get());
  // Ideally this should be done after checking
  // ExtensionAPIEnabledInExtensionServiceWorkers(), but the Dispatcher is
  // created so early that sending an IPC from browser/ process to synchronize
  // this enabled-ness is too late.
  WorkerThreadDispatcher::Get()->Init(RenderThread::Get());

  // Register WebSecurityPolicy whitelists for the chrome-extension:// scheme.
  WebString extension_scheme(WebString::FromASCII(kExtensionScheme));

  // Extension resources are HTTP-like and safe to expose to the fetch API. The
  // rules for the fetch API are consistent with XHR.
  WebSecurityPolicy::RegisterURLSchemeAsSupportingFetchAPI(extension_scheme);

  // Extension resources, when loaded as the top-level document, should bypass
  // Blink's strict first-party origin checks.
  WebSecurityPolicy::RegisterURLSchemeAsFirstPartyWhenTopLevel(
      extension_scheme);

  // Disallow running javascript URLs on the chrome-extension scheme.
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      extension_scheme);

  // Initialize host permissions for any extensions that were activated before
  // WebKit was initialized.
  for (const std::string& extension_id : active_extension_ids_) {
    const Extension* extension =
        RendererExtensionRegistry::Get()->GetByID(extension_id);
    CHECK(extension);
    InitOriginPermissions(extension);
  }

  EnableCustomElementWhiteList();
}

Dispatcher::~Dispatcher() {
}

void Dispatcher::OnRenderThreadStarted(content::RenderThread* thread) {
  thread->RegisterExtension(extensions::SafeBuiltins::CreateV8Extension());
}

void Dispatcher::OnRenderFrameCreated(content::RenderFrame* render_frame) {
  script_injection_manager_->OnRenderFrameCreated(render_frame);
  content_watcher_->OnRenderFrameCreated(render_frame);
}

bool Dispatcher::IsExtensionActive(const std::string& extension_id) const {
  bool is_active =
      active_extension_ids_.find(extension_id) != active_extension_ids_.end();
  if (is_active)
    CHECK(RendererExtensionRegistry::Get()->Contains(extension_id));
  return is_active;
}

void Dispatcher::DidCreateScriptContext(
    blink::WebLocalFrame* frame,
    const v8::Local<v8::Context>& v8_context,
    int world_id) {
  const base::TimeTicks start_time = base::TimeTicks::Now();

  ScriptContext* context =
      script_context_set_->Register(frame, v8_context, world_id);

  // Initialize origin permissions for content scripts, which can't be
  // initialized in |OnActivateExtension|.
  if (context->context_type() == Feature::CONTENT_SCRIPT_CONTEXT)
    InitOriginPermissions(context->extension());

  {
    std::unique_ptr<ModuleSystem> module_system(
        new ModuleSystem(context, &source_map_));
    context->SetModuleSystem(std::move(module_system));
  }
  ModuleSystem* module_system = context->module_system();

  // Enable natives in startup.
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system);

  RegisterNativeHandlers(module_system, context, bindings_system_.get(),
                         v8_schema_registry_.get());

  bindings_system_->DidCreateScriptContext(context);
  UpdateBindingsForContext(context);

  // Inject custom JS into the platform app context.
  if (IsWithinPlatformApp()) {
    module_system->Require("platformApp");
  }

  RequireGuestViewModules(context);

  const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
  switch (context->context_type()) {
    case Feature::UNSPECIFIED_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_Unspecified",
                          elapsed);
      break;
    case Feature::BLESSED_EXTENSION_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_Blessed", elapsed);
      break;
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_Unblessed",
                          elapsed);
      break;
    case Feature::CONTENT_SCRIPT_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_ContentScript",
                          elapsed);
      break;
    case Feature::WEB_PAGE_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_WebPage", elapsed);
      break;
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_BlessedWebPage",
                          elapsed);
      break;
    case Feature::WEBUI_CONTEXT:
      UMA_HISTOGRAM_TIMES("Extensions.DidCreateScriptContext_WebUI", elapsed);
      break;
    case Feature::SERVICE_WORKER_CONTEXT:
      // Handled in DidInitializeServiceWorkerContextOnWorkerThread().
      NOTREACHED();
      break;
    case Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
      UMA_HISTOGRAM_TIMES(
          "Extensions.DidCreateScriptContext_LockScreenExtension", elapsed);
      break;
  }

  VLOG(1) << "Num tracked contexts: " << script_context_set_->size();
}

void Dispatcher::DidInitializeServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> v8_context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
  const base::TimeTicks start_time = base::TimeTicks::Now();

  if (!script_url.SchemeIs(kExtensionScheme)) {
    // Early-out if this isn't a chrome-extension:// scheme, because looking up
    // the extension registry is unnecessary if it's not. Checking this will
    // also skip over hosted apps, which is the desired behavior - hosted app
    // service workers are not our concern.
    return;
  }

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(script_url);

  if (!extension) {
    // TODO(kalman): This is no good. Instead we need to either:
    //
    // - Hold onto the v8::Context and create the ScriptContext and install
    //   our bindings when this extension is loaded.
    // - Deal with there being an extension ID (script_url.host()) but no
    //   extension associated with it, then document that getBackgroundClient
    //   may fail if the extension hasn't loaded yet.
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

  ScriptContext* context = new ScriptContext(
      v8_context, nullptr, extension, Feature::SERVICE_WORKER_CONTEXT,
      extension, Feature::SERVICE_WORKER_CONTEXT);
  context->set_url(script_url);
  context->set_service_worker_scope(service_worker_scope);
  context->set_service_worker_version_id(service_worker_version_id);

  if (ExtensionsClient::Get()->ExtensionAPIEnabledInExtensionServiceWorkers()) {
    WorkerThreadDispatcher* worker_dispatcher = WorkerThreadDispatcher::Get();
    std::unique_ptr<IPCMessageSender> ipc_sender =
        IPCMessageSender::CreateWorkerThreadIPCMessageSender(
            worker_dispatcher, service_worker_version_id);
    worker_dispatcher->AddWorkerData(
        service_worker_version_id, context,
        CreateBindingsSystem(std::move(ipc_sender)));

    // TODO(lazyboy): Make sure accessing |source_map_| in worker thread is
    // safe.
    context->SetModuleSystem(
        std::make_unique<ModuleSystem>(context, &source_map_));

    ModuleSystem* module_system = context->module_system();
    // Enable natives in startup.
    ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system);
    ExtensionBindingsSystem* worker_bindings_system =
        WorkerThreadDispatcher::GetBindingsSystem();
    RegisterNativeHandlers(module_system, context, worker_bindings_system,
                           WorkerThreadDispatcher::GetV8SchemaRegistry());

    worker_bindings_system->DidCreateScriptContext(context);
    worker_bindings_system->UpdateBindingsForContext(context);

    // TODO(lazyboy): Get rid of RequireGuestViewModules() as this doesn't seem
    // necessary for Extension SW.
    RequireGuestViewModules(context);
  }

  g_worker_script_context_set.Get().Insert(base::WrapUnique(context));

  v8::Isolate* isolate = context->isolate();

  // Fetch the source code for service_worker_bindings.js.
  base::StringPiece script_resource =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SERVICE_WORKER_BINDINGS_JS);
  v8::Local<v8::String> script = v8::String::NewExternal(
      isolate, new StaticV8ExternalOneByteStringResource(script_resource));

  // Run service_worker.js to get the main function.
  v8::Local<v8::Function> main_function;
  {
    v8::Local<v8::Value> result = context->RunScript(
        v8_helpers::ToV8StringUnsafe(isolate, "service_worker"), script,
        base::Bind(&CrashOnException));
    CHECK(result->IsFunction());
    main_function = result.As<v8::Function>();
  }

  // Expose CHECK/DCHECK/NOTREACHED to the main function with a
  // LoggingNativeHandler. Admire the neat base::Bind trick to both Invalidate
  // and delete the native handler.
  LoggingNativeHandler* logging = new LoggingNativeHandler(context);
  logging->Initialize();
  context->AddInvalidationObserver(
      base::BindOnce(&NativeHandler::Invalidate, base::Owned(logging)));

  // Execute the main function with its dependencies passed in as arguments.
  v8::Local<v8::Value> args[] = {
      // The extension's background URL.
      v8_helpers::ToV8StringUnsafe(
          isolate, BackgroundInfo::GetBackgroundURL(extension).spec()),
      // The wake-event-page native function.
      WakeEventPage::Get()->GetForContext(context),
      // The logging module.
      logging->NewInstance(),
  };
  context->SafeCallFunction(main_function, arraysize(args), args);

  const base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES(
      "Extensions.DidInitializeServiceWorkerContextOnWorkerThread", elapsed);
}

void Dispatcher::WillReleaseScriptContext(
    blink::WebLocalFrame* frame,
    const v8::Local<v8::Context>& v8_context,
    int world_id) {
  ScriptContext* context = script_context_set_->GetByV8Context(v8_context);
  if (!context)
    return;
  bindings_system_->WillReleaseScriptContext(context);

  script_context_set_->Remove(context);
  VLOG(1) << "Num tracked contexts: " << script_context_set_->size();
}

// static
void Dispatcher::DidStartServiceWorkerContextOnWorkerThread(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
  if (!script_url.SchemeIs(kExtensionScheme)) {
    // See comment in DidInitializeServiceWorkerContextOnWorkerThread.
    return;
  }
  if (!ExtensionsClient::Get()->ExtensionAPIEnabledInExtensionServiceWorkers())
    return;

  DCHECK_NE(content::WorkerThread::GetCurrentId(), kMainThreadId);
  WorkerThreadDispatcher::Get()->DidStartContext(service_worker_version_id);
}

// static
void Dispatcher::WillDestroyServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> v8_context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
  if (!script_url.SchemeIs(kExtensionScheme)) {
    // See comment in DidInitializeServiceWorkerContextOnWorkerThread.
    return;
  }

  if (ExtensionsClient::Get()->ExtensionAPIEnabledInExtensionServiceWorkers()) {
    // TODO(lazyboy/devlin): Should this cleanup happen in a worker class, like
    // WorkerThreadDispatcher? If so, we should move the initialization as well.
    ScriptContext* script_context = WorkerThreadDispatcher::GetScriptContext();
    ExtensionBindingsSystem* worker_bindings_system =
        WorkerThreadDispatcher::GetBindingsSystem();
    worker_bindings_system->WillReleaseScriptContext(script_context);
    WorkerThreadDispatcher::Get()->DidStopContext(service_worker_version_id);
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
}

void Dispatcher::DidCreateDocumentElement(blink::WebLocalFrame* frame) {
  // Note: use GetEffectiveDocumentURL not just frame->document()->url()
  // so that this also injects the stylesheet on about:blank frames that
  // are hosted in the extension process.
  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      frame, frame->GetDocument().Url(), true /* match_about_blank */);

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          effective_document_url);

  if (extension &&
      (extension->is_extension() || extension->is_platform_app())) {
    int resource_id = extension->is_platform_app() ? IDR_PLATFORM_APP_CSS
                                                   : IDR_EXTENSION_FONTS_CSS;
    std::string stylesheet = ui::ResourceBundle::GetSharedInstance()
                                 .GetRawDataResource(resource_id)
                                 .as_string();
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
    base::StringPiece extension_css =
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_EXTENSION_CSS);
    frame->GetDocument().InsertStyleSheet(
        WebString::FromUTF8(extension_css.data(), extension_css.length()));
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

void Dispatcher::OnExtensionResponse(int request_id,
                                     bool success,
                                     const base::ListValue& response,
                                     const std::string& error) {
  bindings_system_->HandleResponse(request_id, success, response, error);
}

void Dispatcher::DispatchEvent(const std::string& extension_id,
                               const std::string& event_name,
                               const base::ListValue& event_args,
                               const EventFilteringInfo* filtering_info) const {
  script_context_set_->ForEach(
      extension_id, nullptr,
      base::Bind(&ExtensionBindingsSystem::DispatchEventInContext,
                 base::Unretained(bindings_system_.get()), event_name,
                 &event_args, filtering_info));
}

void Dispatcher::InvokeModuleSystemMethod(content::RenderFrame* render_frame,
                                          const std::string& extension_id,
                                          const std::string& module_name,
                                          const std::string& function_name,
                                          const base::ListValue& args) {
  script_context_set_->ForEach(
      extension_id, render_frame,
      base::Bind(&CallModuleMethod, module_name, function_name, &args));
}

// static
std::vector<Dispatcher::JsResourceInfo> Dispatcher::GetJsResources() {
  // Libraries.
  std::vector<JsResourceInfo> resources = {
      {"appView", IDR_APP_VIEW_JS},
      {"appViewElement", IDR_APP_VIEW_ELEMENT_JS},
      {"entryIdManager", IDR_ENTRY_ID_MANAGER},
      {"extensionOptions", IDR_EXTENSION_OPTIONS_JS},
      {"extensionOptionsElement", IDR_EXTENSION_OPTIONS_ELEMENT_JS},
      {"extensionOptionsAttributes", IDR_EXTENSION_OPTIONS_ATTRIBUTES_JS},
      {"extensionOptionsConstants", IDR_EXTENSION_OPTIONS_CONSTANTS_JS},
      {"extensionOptionsEvents", IDR_EXTENSION_OPTIONS_EVENTS_JS},
      {"extensionView", IDR_EXTENSION_VIEW_JS},
      {"extensionViewElement", IDR_EXTENSION_VIEW_ELEMENT_JS},
      {"extensionViewApiMethods", IDR_EXTENSION_VIEW_API_METHODS_JS},
      {"extensionViewAttributes", IDR_EXTENSION_VIEW_ATTRIBUTES_JS},
      {"extensionViewConstants", IDR_EXTENSION_VIEW_CONSTANTS_JS},
      {"extensionViewEvents", IDR_EXTENSION_VIEW_EVENTS_JS},
      {"extensionViewInternal", IDR_EXTENSION_VIEW_INTERNAL_CUSTOM_BINDINGS_JS},
      {"feedbackPrivate", IDR_FEEDBACK_PRIVATE_CUSTOM_BINDINGS_JS},
      {"fileEntryBindingUtil", IDR_FILE_ENTRY_BINDING_UTIL_JS},
      {"fileSystem", IDR_FILE_SYSTEM_CUSTOM_BINDINGS_JS},
      {"guestView", IDR_GUEST_VIEW_JS},
      {"guestViewAttributes", IDR_GUEST_VIEW_ATTRIBUTES_JS},
      {"guestViewContainer", IDR_GUEST_VIEW_CONTAINER_JS},
      {"guestViewContainerElement", IDR_GUEST_VIEW_CONTAINER_ELEMENT_JS},
      {"guestViewDeny", IDR_GUEST_VIEW_DENY_JS},
      {"guestViewEvents", IDR_GUEST_VIEW_EVENTS_JS},
      {"imageUtil", IDR_IMAGE_UTIL_JS},
      {"setIcon", IDR_SET_ICON_JS},
      {"test", IDR_TEST_CUSTOM_BINDINGS_JS},
      {"test_environment_specific_bindings",
       IDR_BROWSER_TEST_ENVIRONMENT_SPECIFIC_BINDINGS_JS},
      {"uncaught_exception_handler", IDR_UNCAUGHT_EXCEPTION_HANDLER_JS},
      {"utils", IDR_UTILS_JS},
      {"webRequest", IDR_WEB_REQUEST_CUSTOM_BINDINGS_JS},
      {"webRequestEvent", IDR_WEB_REQUEST_EVENT_JS},
      // Note: webView not webview so that this doesn't interfere with the
      // chrome.webview API bindings.
      {"webView", IDR_WEB_VIEW_JS},
      {"webViewElement", IDR_WEB_VIEW_ELEMENT_JS},
      {"extensionsWebViewElement", IDR_EXTENSIONS_WEB_VIEW_ELEMENT_JS},
      {"webViewActionRequests", IDR_WEB_VIEW_ACTION_REQUESTS_JS},
      {"webViewApiMethods", IDR_WEB_VIEW_API_METHODS_JS},
      {"webViewAttributes", IDR_WEB_VIEW_ATTRIBUTES_JS},
      {"webViewConstants", IDR_WEB_VIEW_CONSTANTS_JS},
      {"webViewEvents", IDR_WEB_VIEW_EVENTS_JS},
      {"webViewInternal", IDR_WEB_VIEW_INTERNAL_CUSTOM_BINDINGS_JS},

      {"keep_alive", IDR_KEEP_ALIVE_JS},
      {"mojo_bindings", IDR_MOJO_MOJO_BINDINGS_JS, true},
      {"extensions/common/mojo/keep_alive.mojom", IDR_KEEP_ALIVE_MOJOM_JS},

      // Custom bindings.
      {"app.runtime", IDR_APP_RUNTIME_CUSTOM_BINDINGS_JS},
      {"app.window", IDR_APP_WINDOW_CUSTOM_BINDINGS_JS},
      {"declarativeWebRequest", IDR_DECLARATIVE_WEBREQUEST_CUSTOM_BINDINGS_JS},
      {"displaySource", IDR_DISPLAY_SOURCE_CUSTOM_BINDINGS_JS},
      {"contextMenus", IDR_CONTEXT_MENUS_CUSTOM_BINDINGS_JS},
      {"contextMenusHandlers", IDR_CONTEXT_MENUS_HANDLERS_JS},
      {"mimeHandlerPrivate", IDR_MIME_HANDLER_PRIVATE_CUSTOM_BINDINGS_JS},
      {"extensions/common/api/mime_handler.mojom", IDR_MIME_HANDLER_MOJOM_JS},
      {"mojoPrivate", IDR_MOJO_PRIVATE_CUSTOM_BINDINGS_JS},
      {"permissions", IDR_PERMISSIONS_CUSTOM_BINDINGS_JS},
      {"printerProvider", IDR_PRINTER_PROVIDER_CUSTOM_BINDINGS_JS},
      {"webViewRequest", IDR_WEB_VIEW_REQUEST_CUSTOM_BINDINGS_JS},

      // Platform app sources that are not API-specific..
      {"platformApp", IDR_PLATFORM_APP_JS},
  };

  if (!base::FeatureList::IsEnabled(extensions_features::kNativeCrxBindings)) {
    resources.push_back({"binding", IDR_BINDING_JS});
    resources.push_back({kEventBindings, IDR_EVENT_BINDINGS_JS});
    resources.push_back({"lastError", IDR_LAST_ERROR_JS});
    resources.push_back({"sendRequest", IDR_SEND_REQUEST_JS});
    resources.push_back({kSchemaUtils, IDR_SCHEMA_UTILS_JS});
    resources.push_back({"json_schema", IDR_JSON_SCHEMA_JS});

    resources.push_back({"messaging", IDR_MESSAGING_JS});
    resources.push_back({"messaging_utils", IDR_MESSAGING_UTILS_JS});
    resources.push_back({"extension", IDR_EXTENSION_CUSTOM_BINDINGS_JS});
    resources.push_back({"i18n", IDR_I18N_CUSTOM_BINDINGS_JS});
    resources.push_back({"runtime", IDR_RUNTIME_CUSTOM_BINDINGS_JS});

    // Custom types sources.
    resources.push_back({"StorageArea", IDR_STORAGE_AREA_JS});
  }

  if (base::FeatureList::IsEnabled(::features::kGuestViewCrossProcessFrames)) {
    resources.push_back({"guestViewIframe", IDR_GUEST_VIEW_IFRAME_JS});
    resources.push_back(
        {"guestViewIframeContainer", IDR_GUEST_VIEW_IFRAME_CONTAINER_JS});
  }

  return resources;
}

// NOTE: please use the naming convention "foo_natives" for these.
// static
void Dispatcher::RegisterNativeHandlers(
    ModuleSystem* module_system,
    ScriptContext* context,
    Dispatcher* dispatcher,
    ExtensionBindingsSystem* bindings_system,
    V8SchemaRegistry* v8_schema_registry) {
  module_system->RegisterNativeHandler(
      "chrome",
      std::unique_ptr<NativeHandler>(new ChromeNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "logging",
      std::unique_ptr<NativeHandler>(new LoggingNativeHandler(context)));
  module_system->RegisterNativeHandler("schema_registry",
                                       v8_schema_registry->AsNativeHandler());
  module_system->RegisterNativeHandler(
      "test_features",
      std::unique_ptr<NativeHandler>(new TestFeaturesNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "test_native_handler",
      std::unique_ptr<NativeHandler>(new TestNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "user_gestures",
      std::unique_ptr<NativeHandler>(new UserGesturesNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "utils", std::unique_ptr<NativeHandler>(new UtilsNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "v8_context",
      std::unique_ptr<NativeHandler>(new V8ContextNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "event_natives",
      std::make_unique<EventBindings>(
          context,
          // Note: |bindings_system| can be null in unit tests.
          bindings_system ? bindings_system->GetIPCMessageSender() : nullptr));
  module_system->RegisterNativeHandler(
      "messaging_natives", std::make_unique<MessagingBindings>(context));
  module_system->RegisterNativeHandler(
      "apiDefinitions", std::unique_ptr<NativeHandler>(
                            new ApiDefinitionsNatives(dispatcher, context)));
  module_system->RegisterNativeHandler(
      "sendRequest",
      std::make_unique<SendRequestNatives>(
          // Note: |bindings_system| can be null in unit tests.
          bindings_system ? bindings_system->GetRequestSender() : nullptr,
          context));
  module_system->RegisterNativeHandler(
      "setIcon", std::unique_ptr<NativeHandler>(new SetIconNatives(context)));
  module_system->RegisterNativeHandler(
      "activityLogger", std::make_unique<APIActivityLogger>(context));
  module_system->RegisterNativeHandler(
      "renderFrameObserverNatives",
      std::unique_ptr<NativeHandler>(new RenderFrameObserverNatives(context)));

  // Natives used by multiple APIs.
  module_system->RegisterNativeHandler(
      "file_system_natives",
      std::unique_ptr<NativeHandler>(new FileSystemNatives(context)));

  // Custom bindings.
  module_system->RegisterNativeHandler(
      "app_window_natives",
      std::unique_ptr<NativeHandler>(new AppWindowCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "blob_natives",
      std::unique_ptr<NativeHandler>(new BlobNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "context_menus",
      std::unique_ptr<NativeHandler>(new ContextMenusCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "guest_view_internal", std::unique_ptr<NativeHandler>(
                                 new GuestViewInternalCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "id_generator",
      std::unique_ptr<NativeHandler>(new IdGeneratorCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "runtime",
      std::unique_ptr<NativeHandler>(new RuntimeCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "display_source",
      std::make_unique<DisplaySourceCustomBindings>(context, bindings_system));
}

bool Dispatcher::OnControlMessageReceived(const IPC::Message& message) {
  if (WorkerThreadDispatcher::Get()->OnControlMessageReceived(message))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(Dispatcher, message)
  IPC_MESSAGE_HANDLER(ExtensionMsg_ActivateExtension, OnActivateExtension)
  IPC_MESSAGE_HANDLER(ExtensionMsg_CancelSuspend, OnCancelSuspend)
  IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnDeliverMessage)
  IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnConnect, OnDispatchOnConnect)
  IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnDisconnect, OnDispatchOnDisconnect)
  IPC_MESSAGE_HANDLER(ExtensionMsg_Loaded, OnLoaded)
  IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnMessageInvoke)
  IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchEvent, OnDispatchEvent)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetSessionInfo, OnSetSessionInfo)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetScriptingWhitelist,
                      OnSetScriptingWhitelist)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetSystemFont, OnSetSystemFont)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetWebViewPartitionID,
                      OnSetWebViewPartitionID)
  IPC_MESSAGE_HANDLER(ExtensionMsg_ShouldSuspend, OnShouldSuspend)
  IPC_MESSAGE_HANDLER(ExtensionMsg_Suspend, OnSuspend)
  IPC_MESSAGE_HANDLER(ExtensionMsg_TransferBlobs, OnTransferBlobs)
  IPC_MESSAGE_HANDLER(ExtensionMsg_Unloaded, OnUnloaded)
  IPC_MESSAGE_HANDLER(ExtensionMsg_UpdatePermissions, OnUpdatePermissions)
  IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateDefaultPolicyHostRestrictions,
                      OnUpdateDefaultPolicyHostRestrictions)
  IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateTabSpecificPermissions,
                      OnUpdateTabSpecificPermissions)
  IPC_MESSAGE_HANDLER(ExtensionMsg_ClearTabSpecificPermissions,
                      OnClearTabSpecificPermissions)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetActivityLoggingEnabled,
                      OnSetActivityLoggingEnabled)
  IPC_MESSAGE_FORWARD(ExtensionMsg_WatchPages,
                      content_watcher_.get(),
                      ContentWatcher::OnWatchPages)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void Dispatcher::OnActivateExtension(const std::string& extension_id) {
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension) {
    NOTREACHED();
    // Extension was activated but was never loaded. This probably means that
    // the renderer failed to load it (or the browser failed to tell us when it
    // did). Failures shouldn't happen, but instead of crashing there (which
    // executes on all renderers) be conservative and only crash in the renderer
    // of the extension which failed to load; this one.
    std::string& error = extension_load_errors_[extension_id];
    char minidump[256];
    base::debug::Alias(&minidump);
    base::snprintf(minidump,
                   arraysize(minidump),
                   "e::dispatcher:%s:%s",
                   extension_id.c_str(),
                   error.c_str());
    LOG(FATAL) << extension_id << " was never loaded: " << error;
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

void Dispatcher::OnCancelSuspend(const std::string& extension_id) {
  DispatchEvent(extension_id, kOnSuspendCanceledEvent, base::ListValue(),
                nullptr);
}

void Dispatcher::OnDeliverMessage(const PortId& target_port_id,
                                  const Message& message) {
  bindings_system_->GetMessagingService()->DeliverMessage(
      *script_context_set_, target_port_id, message,
      NULL);  // All render frames.
}

void Dispatcher::OnDispatchOnConnect(
    const PortId& target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo& source,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id) {
  DCHECK(!target_port_id.is_opener);

  bindings_system_->GetMessagingService()->DispatchOnConnect(
      *script_context_set_, target_port_id, channel_name, source, info,
      tls_channel_id,
      NULL);  // All render frames.
}

void Dispatcher::OnDispatchOnDisconnect(const PortId& port_id,
                                        const std::string& error_message) {
  bindings_system_->GetMessagingService()->DispatchOnDisconnect(
      *script_context_set_, port_id, error_message,
      NULL);  // All render frames.
}

void Dispatcher::OnLoaded(
    const std::vector<ExtensionMsg_Loaded_Params>& loaded_extensions) {
  for (const auto& param : loaded_extensions) {
    std::string error;
    scoped_refptr<const Extension> extension = param.ConvertToExtension(&error);
    if (!extension.get()) {
      NOTREACHED() << error;
      // Note: in tests |param.id| has been observed to be empty (see comment
      // just below) so this isn't all that reliable.
      extension_load_errors_[param.id] = error;
      continue;
    }
    RendererExtensionRegistry* extension_registry =
        RendererExtensionRegistry::Get();
    // TODO(kalman): This test is deliberately not a CHECK (though I wish it
    // could be) and uses extension->id() not params.id:
    // 1. For some reason params.id can be empty. I've only seen it with
    //    the webstore extension, in tests, and I've spent some time trying to
    //    figure out why - but cost/benefit won.
    // 2. The browser only sends this IPC to RenderProcessHosts once, but the
    //    Dispatcher is attached to a RenderThread. Presumably there is a
    //    mismatch there. In theory one would think it's possible for the
    //    browser to figure this out itself - but again, cost/benefit.
    if (!extension_registry->Insert(extension)) {
      // TODO(devlin): This may be fixed by crbug.com/528026. Monitor, and
      // consider making this a release CHECK.
      NOTREACHED();
    }
    if (param.uses_default_policy_blocked_allowed_hosts) {
      extension->permissions_data()->SetUsesDefaultHostRestrictions();
    } else {
      extension->permissions_data()->SetPolicyHostRestrictions(
          param.policy_blocked_hosts, param.policy_allowed_hosts);
    }

    ExtensionsRendererClient::Get()->OnExtensionLoaded(*extension);
  }

  // Update the available bindings for all contexts. These may have changed if
  // an externally_connectable extension was loaded that can connect to an
  // open webpage.
  UpdateBindings(std::string());
}

void Dispatcher::OnMessageInvoke(const std::string& extension_id,
                                 const std::string& module_name,
                                 const std::string& function_name,
                                 const base::ListValue& args) {
  InvokeModuleSystemMethod(nullptr, extension_id, module_name, function_name,
                           args);
}

void Dispatcher::OnDispatchEvent(
    const ExtensionMsg_DispatchEvent_Params& params,
    const base::ListValue& event_args) {
  std::unique_ptr<WebScopedUserGesture> web_user_gesture;
  if (params.is_user_gesture)
    web_user_gesture.reset(new WebScopedUserGesture(nullptr));

  DispatchEvent(params.extension_id, params.event_name, event_args,
                &params.filtering_info);

  // Tell the browser process when an event has been dispatched with a lazy
  // background page active.
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(params.extension_id);
  if (extension && BackgroundInfo::HasLazyBackgroundPage(extension)) {
    content::RenderFrame* background_frame =
        ExtensionFrameHelper::GetBackgroundPageFrame(params.extension_id);
    if (background_frame) {
      background_frame->Send(new ExtensionHostMsg_EventAck(
          background_frame->GetRoutingID(), params.event_id));
    }
  }
}

void Dispatcher::OnSetSessionInfo(version_info::Channel channel,
                                  FeatureSessionType session_type,
                                  bool is_lock_screen_context) {
  SetCurrentChannel(channel);
  SetCurrentFeatureSessionType(session_type);
  script_context_set_->set_is_lock_screen_context(is_lock_screen_context);

  if (feature_util::ExtensionServiceWorkersEnabled()) {
    // chrome-extension: resources should be allowed to register ServiceWorkers.
    blink::WebSecurityPolicy::RegisterURLSchemeAsAllowingServiceWorkers(
        blink::WebString::FromUTF8(extensions::kExtensionScheme));
  }

  blink::WebSecurityPolicy::RegisterURLSchemeAsAllowingWasmEvalCSP(
      blink::WebString::FromUTF8(extensions::kExtensionScheme));
}

void Dispatcher::OnSetScriptingWhitelist(
    const ExtensionsClient::ScriptingWhitelist& extension_ids) {
  ExtensionsClient::Get()->SetScriptingWhitelist(extension_ids);
}

void Dispatcher::OnSetSystemFont(const std::string& font_family,
                                 const std::string& font_size) {
  system_font_family_ = font_family;
  system_font_size_ = font_size;
}

void Dispatcher::OnSetWebViewPartitionID(const std::string& partition_id) {
  // |webview_partition_id_| cannot be changed once set.
  CHECK(webview_partition_id_.empty() || webview_partition_id_ == partition_id);
  webview_partition_id_ = partition_id;
}

void Dispatcher::OnShouldSuspend(const std::string& extension_id,
                                 uint64_t sequence_id) {
  RenderThread::Get()->Send(
      new ExtensionHostMsg_ShouldSuspendAck(extension_id, sequence_id));
}

void Dispatcher::OnSuspend(const std::string& extension_id) {
  // Dispatch the suspend event. This doesn't go through the standard event
  // dispatch machinery because it requires special handling. We need to let
  // the browser know when we are starting and stopping the event dispatch, so
  // that it still considers the extension idle despite any activity the suspend
  // event creates.
  DispatchEvent(extension_id, kOnSuspendEvent, base::ListValue(), nullptr);
  RenderThread::Get()->Send(new ExtensionHostMsg_SuspendAck(extension_id));
}

void Dispatcher::OnTransferBlobs(const std::vector<std::string>& blob_uuids) {
  RenderThread::Get()->Send(new ExtensionHostMsg_TransferBlobsAck(blob_uuids));
}

void Dispatcher::OnUnloaded(const std::string& id) {
  // See comment in OnLoaded for why it would be nice, but perhaps incorrect,
  // to CHECK here rather than guarding.
  // TODO(devlin): This may be fixed by crbug.com/528026. Monitor, and
  // consider making this a release CHECK.
  if (!RendererExtensionRegistry::Get()->Remove(id)) {
    NOTREACHED();
    return;
  }

  ExtensionsRendererClient::Get()->OnExtensionUnloaded(id);

  bindings_system_->OnExtensionRemoved(id);

  active_extension_ids_.erase(id);

  script_injection_manager_->OnExtensionUnloaded(id);

  // If the extension is later reloaded with a different set of permissions,
  // we'd like it to get a new isolated world ID, so that it can pick up the
  // changed origin whitelist.
  ScriptInjection::RemoveIsolatedWorld(id);

  // Inform the bindings system that the contexts will be removed to allow time
  // to clear out context-specific data, and then remove the contexts
  // themselves.
  script_context_set_->ForEach(
      id, nullptr,
      base::Bind(&ExtensionBindingsSystem::WillReleaseScriptContext,
                 base::Unretained(bindings_system_.get())));
  script_context_set_->OnExtensionUnloaded(id);

  // Update the available bindings for the remaining contexts. These may have
  // changed if an externally_connectable extension is unloaded and a webpage
  // is no longer accessible.
  UpdateBindings("");

  // Invalidates the messages map for the extension in case the extension is
  // reloaded with a new messages map.
  EraseL10nMessagesMap(id);

  // Update the origin access map so that any content scripts injected are no
  // longer allowlisted for extra origins.
  WebSecurityPolicy::ClearOriginAccessAllowListForOrigin(
      Extension::GetBaseURLFromExtensionId(id));

  // We don't do anything with existing platform-app stylesheets. They will
  // stay resident, but the URL pattern corresponding to the unloaded
  // extension's URL just won't match anything anymore.
}

void Dispatcher::OnUpdateDefaultPolicyHostRestrictions(
    const ExtensionMsg_UpdateDefaultPolicyHostRestrictions_Params& params) {
  PermissionsData::SetDefaultPolicyHostRestrictions(
      params.default_policy_blocked_hosts, params.default_policy_allowed_hosts);
  // Update blink host permission allowlist exceptions for all loaded
  // extensions.
  for (const std::string& extension_id :
       RendererExtensionRegistry::Get()->GetIDs()) {
    const Extension* extension =
        RendererExtensionRegistry::Get()->GetByID(extension_id);
    if (extension->permissions_data()->UsesDefaultPolicyHostRestrictions()) {
      UpdateOriginPermissions(*extension);
    }
  }
  UpdateBindings(std::string());
}

void Dispatcher::OnUpdatePermissions(
    const ExtensionMsg_UpdatePermissions_Params& params) {
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(params.extension_id);
  if (!extension)
    return;

  if (params.uses_default_policy_host_restrictions) {
    extension->permissions_data()->SetUsesDefaultHostRestrictions();
  } else {
    extension->permissions_data()->SetPolicyHostRestrictions(
        params.policy_blocked_hosts, params.policy_allowed_hosts);
  }

  std::unique_ptr<const PermissionSet> active =
      params.active_permissions.ToPermissionSet();
  std::unique_ptr<const PermissionSet> withheld =
      params.withheld_permissions.ToPermissionSet();

  extension->permissions_data()->SetPermissions(std::move(active),
                                                std::move(withheld));
  UpdateOriginPermissions(*extension);

  if (params.uses_default_policy_host_restrictions) {
    extension->permissions_data()->SetUsesDefaultHostRestrictions();
  } else {
    extension->permissions_data()->SetPolicyHostRestrictions(
        params.policy_blocked_hosts, params.policy_allowed_hosts);
  }

  bindings_system_->OnExtensionPermissionsUpdated(params.extension_id);
  UpdateBindings(extension->id());
}

void Dispatcher::OnUpdateTabSpecificPermissions(const GURL& visible_url,
                                                const std::string& extension_id,
                                                const URLPatternSet& new_hosts,
                                                bool update_origin_whitelist,
                                                int tab_id) {
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension)
    return;

  extension->permissions_data()->UpdateTabSpecificPermissions(
      tab_id, extensions::PermissionSet(extensions::APIPermissionSet(),
                                        extensions::ManifestPermissionSet(),
                                        new_hosts, new_hosts));

  if (update_origin_whitelist)
    UpdateOriginPermissions(*extension);
}

void Dispatcher::OnClearTabSpecificPermissions(
    const std::vector<std::string>& extension_ids,
    bool update_origin_whitelist,
    int tab_id) {
  for (const std::string& id : extension_ids) {
    const Extension* extension = RendererExtensionRegistry::Get()->GetByID(id);
    if (extension) {
      extension->permissions_data()->ClearTabSpecificPermissions(tab_id);
      if (update_origin_whitelist)
        UpdateOriginPermissions(*extension);
    }
  }
}

void Dispatcher::OnSetActivityLoggingEnabled(bool enabled) {
  activity_logging_enabled_ = enabled;
  if (enabled) {
    for (const std::string& id : active_extension_ids_)
      DOMActivityLogger::AttachToWorld(DOMActivityLogger::kMainWorldId, id);
  }
  script_injection_manager_->set_activity_logging_enabled(enabled);
  user_script_set_manager_->set_activity_logging_enabled(enabled);
}

void Dispatcher::OnUserScriptsUpdated(const std::set<HostID>& changed_hosts) {
  UpdateActiveExtensions();
}

void Dispatcher::UpdateActiveExtensions() {
  std::set<std::string> active_extensions = active_extension_ids_;
  user_script_set_manager_->GetAllActiveExtensionIds(&active_extensions);
  delegate_->OnActiveExtensionsUpdated(active_extensions);
}

void Dispatcher::InitOriginPermissions(const Extension* extension) {
  UpdateOriginPermissions(*extension);
}

void Dispatcher::UpdateOriginPermissions(const Extension& extension) {
  static const char* kSchemes[] = {
    url::kHttpScheme,
    url::kHttpsScheme,
    url::kFileScheme,
    content::kChromeUIScheme,
    url::kFtpScheme,
#if defined(OS_CHROMEOS)
    content::kExternalFileScheme,
#endif
    extensions::kExtensionScheme,
  };

  // Remove all old patterns associated with this extension.
  WebSecurityPolicy::ClearOriginAccessListForOrigin(extension.url());

  delegate_->AddOriginAccessPermissions(extension,
                                        IsExtensionActive(extension.id()));

  URLPatternSet origin_permissions =
      extension.permissions_data()->GetEffectiveHostPermissions();

  // Permissions declared by the extension.
  for (const URLPattern& pattern : origin_permissions) {
    for (const char* scheme : kSchemes) {
      if (pattern.MatchesScheme(scheme))
        WebSecurityPolicy::AddOriginAccessAllowListEntry(
            extension.url(), WebString::FromUTF8(scheme),
            WebString::FromUTF8(pattern.host()), pattern.match_subdomains(),
            network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
    }
  }

  // Hosts blocked by enterprise policy.
  for (const URLPattern& pattern :
       extension.permissions_data()->policy_blocked_hosts()) {
    for (const char* scheme : kSchemes) {
      if (pattern.MatchesScheme(scheme))
        WebSecurityPolicy::AddOriginAccessBlockListEntry(
            extension.url(), WebString::FromUTF8(scheme),
            WebString::FromUTF8(pattern.host()), pattern.match_subdomains(),
            network::mojom::CORSOriginAccessMatchPriority::kLowPriority);
    }
  }

  // Hosts exempted from the enterprise policy blocklist.
  // This set intersection is necessary to prevent an enterprise policy from
  // granting a host permission the extension didn't ask for.
  URLPatternSet overlap = URLPatternSet::CreateIntersection(
      extension.permissions_data()->policy_allowed_hosts(), origin_permissions,
      URLPatternSet::IntersectionBehavior::kDetailed);
  for (const URLPattern& pattern : overlap) {
    for (const char* scheme : kSchemes) {
      if (pattern.MatchesScheme(scheme))
        WebSecurityPolicy::AddOriginAccessAllowListEntry(
            extension.url(), WebString::FromUTF8(scheme),
            WebString::FromUTF8(pattern.host()), pattern.match_subdomains(),
            network::mojom::CORSOriginAccessMatchPriority::kMediumPriority);
    }
  };

  const GURL webstore_launch_url = extension_urls::GetWebstoreLaunchURL();
  WebSecurityPolicy::AddOriginAccessBlockListEntry(
      extension.url(), WebString::FromUTF8(webstore_launch_url.scheme()),
      WebString::FromUTF8(webstore_launch_url.host()), true,
      network::mojom::CORSOriginAccessMatchPriority::kHighPriority);

  // TODO(devlin): Should we also block the webstore update URL here? See
  // https://crbug.com/826946 for a related instance.
}

void Dispatcher::EnableCustomElementWhiteList() {
  blink::WebCustomElement::AddEmbedderCustomElementName("appview");
  blink::WebCustomElement::AddEmbedderCustomElementName("appviewbrowserplugin");
  blink::WebCustomElement::AddEmbedderCustomElementName("extensionoptions");
  blink::WebCustomElement::AddEmbedderCustomElementName(
      "extensionoptionsbrowserplugin");
  blink::WebCustomElement::AddEmbedderCustomElementName("extensionview");
  blink::WebCustomElement::AddEmbedderCustomElementName(
      "extensionviewbrowserplugin");
  blink::WebCustomElement::AddEmbedderCustomElementName("webview");
  blink::WebCustomElement::AddEmbedderCustomElementName("webviewbrowserplugin");
}

void Dispatcher::UpdateBindings(const std::string& extension_id) {
  script_context_set().ForEach(extension_id,
                               base::Bind(&Dispatcher::UpdateBindingsForContext,
                                          base::Unretained(this)));
}

void Dispatcher::UpdateBindingsForContext(ScriptContext* context) {
  bindings_system_->UpdateBindingsForContext(context);
  Feature::Context context_type = context->context_type();
  if (context_type == Feature::WEB_PAGE_CONTEXT ||
      context_type == Feature::BLESSED_WEB_PAGE_CONTEXT) {
    UpdateContentCapabilities(context);
  }
}

// NOTE: please use the naming convention "foo_natives" for these.
void Dispatcher::RegisterNativeHandlers(
    ModuleSystem* module_system,
    ScriptContext* context,
    ExtensionBindingsSystem* bindings_system,
    V8SchemaRegistry* v8_schema_registry) {
  RegisterNativeHandlers(module_system, context, this, bindings_system,
                         v8_schema_registry);
  const Extension* extension = context->extension();
  int manifest_version = extension ? extension->manifest_version() : 1;
  bool is_component_extension =
      extension && Manifest::IsComponentLocation(extension->location());
  bool send_request_disabled = messaging_util::IsSendRequestDisabled(context);
  module_system->RegisterNativeHandler(
      "process",
      std::unique_ptr<NativeHandler>(new ProcessInfoNativeHandler(
          context, context->GetExtensionID(),
          context->GetContextTypeDescription(),
          ExtensionsRendererClient::Get()->IsIncognitoProcess(),
          is_component_extension, manifest_version, send_request_disabled)));

  delegate_->RegisterNativeHandlers(this, module_system, bindings_system,
                                    context);
}

void Dispatcher::UpdateContentCapabilities(ScriptContext* context) {
  APIPermissionSet permissions;
  for (const auto& extension :
       *RendererExtensionRegistry::Get()->GetMainThreadExtensionSet()) {
    blink::WebLocalFrame* web_frame = context->web_frame();
    GURL url = context->url();
    // We allow about:blank pages to take on the privileges of their parents if
    // they aren't sandboxed.
    if (web_frame && !web_frame->GetSecurityOrigin().IsUnique())
      url = ScriptContext::GetEffectiveDocumentURL(web_frame, url, true);
    const ContentCapabilitiesInfo& info =
        ContentCapabilitiesInfo::Get(extension.get());
    if (info.url_patterns.MatchesURL(url)) {
      APIPermissionSet new_permissions;
      APIPermissionSet::Union(permissions, info.permissions, &new_permissions);
      permissions = new_permissions;
    }
  }
  context->set_content_capabilities(permissions);
}

void Dispatcher::PopulateSourceMap() {
  const std::vector<JsResourceInfo> resources = GetJsResources();
  for (const auto& resource : resources)
    source_map_.RegisterSource(resource.name, resource.id, resource.gzipped);
  delegate_->PopulateSourceMap(&source_map_);
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
  Feature::Context context_type = context->context_type();
  ModuleSystem* module_system = context->module_system();
  bool requires_guest_view_module = false;

  // TODO(fsamuel): Eagerly calling Require on context startup is expensive.
  // It would be better if there were a light way of detecting when a webview
  // or appview is created and only then set up the infrastructure.

  // Require AppView.
  if (context->GetAvailability("appViewEmbedderInternal").is_available()) {
    requires_guest_view_module = true;
    module_system->Require("appViewElement");
  }

  // Require ExtensionOptions.
  if (context->GetAvailability("extensionOptionsInternal").is_available()) {
    requires_guest_view_module = true;
    module_system->Require("extensionOptionsElement");
  }

  // Require ExtensionView.
  if (context->GetAvailability("extensionViewInternal").is_available()) {
    requires_guest_view_module = true;
    module_system->Require("extensionViewElement");
  }

  // Require WebView.
  if (context->GetAvailability("webViewInternal").is_available()) {
    requires_guest_view_module = true;
    // The embedder of the extensions layer may define its own implementation
    // of WebView.
    delegate_->RequireWebViewModules(context);
  }

  if (requires_guest_view_module &&
      base::FeatureList::IsEnabled(::features::kGuestViewCrossProcessFrames)) {
    module_system->Require("guestViewIframe");
    module_system->Require("guestViewIframeContainer");
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

  // The "guestViewDeny" module must always be loaded last. It registers
  // error-providing custom elements for the GuestView types that are not
  // available, and thus all of those types must have been checked and loaded
  // (or not loaded) beforehand.
  if (context_type == Feature::BLESSED_EXTENSION_CONTEXT) {
    module_system->Require("guestViewDeny");
  }
}

std::unique_ptr<ExtensionBindingsSystem> Dispatcher::CreateBindingsSystem(
    std::unique_ptr<IPCMessageSender> ipc_sender) {
  std::unique_ptr<ExtensionBindingsSystem> bindings_system;
  if (base::FeatureList::IsEnabled(extensions_features::kNativeCrxBindings)) {
    auto system =
        std::make_unique<NativeExtensionBindingsSystem>(std::move(ipc_sender));
    delegate_->InitializeBindingsSystem(this, system.get());
    bindings_system = std::move(system);
  } else {
    bindings_system = std::make_unique<JsExtensionBindingsSystem>(
        &source_map_, std::move(ipc_sender));
  }
  return bindings_system;
}

}  // namespace extensions
