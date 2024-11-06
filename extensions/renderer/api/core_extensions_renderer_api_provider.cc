// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/core_extensions_renderer_api_provider.h"

#include "extensions/buildflags/buildflags.h"
#include "extensions/grit/extensions_renderer_resources.h"
#include "extensions/renderer/api/context_menus_custom_bindings.h"
#include "extensions/renderer/api/declarative_content_hooks_delegate.h"
#include "extensions/renderer/api/dom_hooks_delegate.h"
#include "extensions/renderer/api/feedback_private_hooks_delegate.h"
#include "extensions/renderer/api/file_system_natives.h"
#include "extensions/renderer/api/i18n_hooks_delegate.h"
#include "extensions/renderer/api/messaging/messaging_bindings.h"
#include "extensions/renderer/api/runtime_hooks_delegate.h"
#include "extensions/renderer/api/web_request_hooks.h"
#include "extensions/renderer/api_activity_logger.h"
#include "extensions/renderer/api_definitions_natives.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/blob_native_handler.h"
#include "extensions/renderer/chrome_setting.h"
#include "extensions/renderer/content_setting.h"
#include "extensions/renderer/id_generator_custom_bindings.h"
#include "extensions/renderer/logging_native_handler.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/process_info_native_handler.h"
#include "extensions/renderer/render_frame_observer_natives.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/runtime_custom_bindings.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/service_worker_natives.h"
#include "extensions/renderer/set_icon_natives.h"
#include "extensions/renderer/storage_area.h"
#include "extensions/renderer/test_features_native_handler.h"
#include "extensions/renderer/user_gestures_native_handler.h"
#include "extensions/renderer/utils_native_handler.h"
#include "extensions/renderer/v8_context_native_handler.h"
#include "extensions/renderer/v8_schema_registry.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"

// TODO(https://crbug.com/356905053): The following files don't compile
// cleanly with the experimental desktop-android build. Either make them
// compile, or determine they should not be included and place them under a
// more appropriate if-block.
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/renderer/api/automation/automation_internal_custom_bindings.h"
#endif

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "extensions/renderer/api/app_window_custom_bindings.h"
#endif

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/renderer/guest_view/guest_view_internal_custom_bindings.h"
#endif

namespace extensions {

void CoreExtensionsRendererAPIProvider::RegisterNativeHandlers(
    ModuleSystem* module_system,
    NativeExtensionBindingsSystem* bindings_system,
    V8SchemaRegistry* v8_schema_registry,
    ScriptContext* context) const {
  module_system->RegisterNativeHandler(
      "logging", std::make_unique<LoggingNativeHandler>(context));
  module_system->RegisterNativeHandler(
      "schema_registry",
      v8_schema_registry->AsNativeHandler(context->isolate()));
  module_system->RegisterNativeHandler(
      "test_features", std::make_unique<TestFeaturesNativeHandler>(context));
  module_system->RegisterNativeHandler(
      "user_gestures", std::make_unique<UserGesturesNativeHandler>(context));
  module_system->RegisterNativeHandler(
      "utils", std::make_unique<UtilsNativeHandler>(context));
  module_system->RegisterNativeHandler(
      "v8_context", std::make_unique<V8ContextNativeHandler>(context));
  module_system->RegisterNativeHandler(
      "messaging_natives", std::make_unique<MessagingBindings>(context));
  module_system->RegisterNativeHandler(
      "apiDefinitions",
      std::make_unique<ApiDefinitionsNatives>(v8_schema_registry, context));
  module_system->RegisterNativeHandler(
      "setIcon", std::make_unique<SetIconNatives>(context));
  module_system->RegisterNativeHandler(
      "activityLogger", std::make_unique<APIActivityLogger>(
                            bindings_system->GetIPCMessageSender(), context));
  module_system->RegisterNativeHandler(
      "renderFrameObserverNatives",
      std::make_unique<RenderFrameObserverNatives>(context));

  // Natives used by multiple APIs.
  module_system->RegisterNativeHandler(
      "file_system_natives", std::make_unique<FileSystemNatives>(context));
  module_system->RegisterNativeHandler(
      "service_worker_natives",
      std::make_unique<ServiceWorkerNatives>(context));

  // Custom bindings.
#if BUILDFLAG(ENABLE_PLATFORM_APPS)
  module_system->RegisterNativeHandler(
      "app_window_natives", std::make_unique<AppWindowCustomBindings>(context));
#endif
  module_system->RegisterNativeHandler(
      "blob_natives", std::make_unique<BlobNativeHandler>(context));
  module_system->RegisterNativeHandler(
      "context_menus", std::make_unique<ContextMenusCustomBindings>(context));

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  module_system->RegisterNativeHandler(
      "guest_view_internal",
      std::make_unique<GuestViewInternalCustomBindings>(context));
#endif

  module_system->RegisterNativeHandler(
      "id_generator", std::make_unique<IdGeneratorCustomBindings>(context));
  module_system->RegisterNativeHandler(
      "process", std::make_unique<ProcessInfoNativeHandler>(context));
  module_system->RegisterNativeHandler(
      "runtime", std::make_unique<RuntimeCustomBindings>(context));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  module_system->RegisterNativeHandler(
      "automationInternal", std::make_unique<AutomationInternalCustomBindings>(
                                context, bindings_system));
#endif
}

void CoreExtensionsRendererAPIProvider::AddBindingsSystemHooks(
    Dispatcher* dispatcher,
    NativeExtensionBindingsSystem* bindings_system) const {
  APIBindingsSystem* bindings = bindings_system->api_system();
  bindings->RegisterCustomType(
      "storage.StorageArea",
      base::BindRepeating(&StorageArea::CreateStorageArea));
  bindings->RegisterCustomType("types.ChromeSetting",
                               base::BindRepeating(&ChromeSetting::Create));
  bindings->RegisterCustomType("contentSettings.ContentSetting",
                               base::BindRepeating(&ContentSetting::Create));
  bindings->RegisterHooksDelegate("webRequest",
                                  std::make_unique<WebRequestHooks>());
  bindings->RegisterHooksDelegate(
      "declarativeContent",
      std::make_unique<DeclarativeContentHooksDelegate>());
  bindings->RegisterHooksDelegate("dom", std::make_unique<DOMHooksDelegate>());
  bindings->RegisterHooksDelegate("i18n",
                                  std::make_unique<I18nHooksDelegate>());
  bindings->RegisterHooksDelegate("runtime",
                                  std::make_unique<RuntimeHooksDelegate>(
                                      bindings_system->messaging_service()));
  bindings->RegisterHooksDelegate(
      "feedbackPrivate", std::make_unique<FeedbackPrivateHooksDelegate>());
}

void CoreExtensionsRendererAPIProvider::PopulateSourceMap(
    ResourceBundleSourceMap* source_map) const {
  static constexpr struct {
    const char* name = nullptr;
    int id = 0;
  } js_resources[] = {
      {"appView", IDR_APP_VIEW_JS},
      {"appViewElement", IDR_APP_VIEW_ELEMENT_JS},
      {"appViewDeny", IDR_APP_VIEW_DENY_JS},
      {"entryIdManager", IDR_ENTRY_ID_MANAGER},
      {"extensionOptions", IDR_EXTENSION_OPTIONS_JS},
      {"extensionOptionsElement", IDR_EXTENSION_OPTIONS_ELEMENT_JS},
      {"extensionOptionsAttributes", IDR_EXTENSION_OPTIONS_ATTRIBUTES_JS},
      {"extensionOptionsConstants", IDR_EXTENSION_OPTIONS_CONSTANTS_JS},
      {"extensionOptionsEvents", IDR_EXTENSION_OPTIONS_EVENTS_JS},
      {"feedbackPrivate", IDR_FEEDBACK_PRIVATE_CUSTOM_BINDINGS_JS},
      {"fileEntryBindingUtil", IDR_FILE_ENTRY_BINDING_UTIL_JS},
      {"fileSystem", IDR_FILE_SYSTEM_CUSTOM_BINDINGS_JS},

#if BUILDFLAG(ENABLE_GUEST_VIEW)
      {"guestView", IDR_GUEST_VIEW_JS},
      {"guestViewAttributes", IDR_GUEST_VIEW_ATTRIBUTES_JS},
      {"guestViewContainer", IDR_GUEST_VIEW_CONTAINER_JS},
      {"guestViewContainerElement", IDR_GUEST_VIEW_CONTAINER_ELEMENT_JS},
      {"guestViewDeny", IDR_GUEST_VIEW_DENY_JS},
      {"guestViewEvents", IDR_GUEST_VIEW_EVENTS_JS},
#endif

      {"safeMethods", IDR_SAFE_METHODS_JS},
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
      {"webViewDeny", IDR_WEB_VIEW_DENY_JS},
      {"webViewActionRequests", IDR_WEB_VIEW_ACTION_REQUESTS_JS},
      {"webViewApiMethods", IDR_WEB_VIEW_API_METHODS_JS},
      {"webViewAttributes", IDR_WEB_VIEW_ATTRIBUTES_JS},
      {"webViewConstants", IDR_WEB_VIEW_CONSTANTS_JS},
      {"webViewEvents", IDR_WEB_VIEW_EVENTS_JS},
      {"webViewInternal", IDR_WEB_VIEW_INTERNAL_CUSTOM_BINDINGS_JS},

      {"keep_alive", IDR_KEEP_ALIVE_JS},

// TODO(https://crbug.com/356905053): Figure out mojo bindings for
// desktop-android builds. Currently, the full bindings aren't generated.
#if BUILDFLAG(ENABLE_EXTENSIONS)
      {"mojo_bindings", IDR_MOJO_MOJO_BINDINGS_JS},
#endif

#if BUILDFLAG(IS_CHROMEOS)
      {"mojo_bindings_lite", IDR_MOJO_MOJO_BINDINGS_LITE_JS},
#endif

      {"extensions/common/mojom/keep_alive.mojom", IDR_KEEP_ALIVE_MOJOM_JS},

      // Custom bindings.
      {"automation", IDR_AUTOMATION_CUSTOM_BINDINGS_JS},
      {"automationEvent", IDR_AUTOMATION_EVENT_JS},
      {"automationNode", IDR_AUTOMATION_NODE_JS},
      {"automationTreeCache", IDR_AUTOMATION_TREE_CACHE_JS},
      {"app.runtime", IDR_APP_RUNTIME_CUSTOM_BINDINGS_JS},
      {"app.window", IDR_APP_WINDOW_CUSTOM_BINDINGS_JS},
      {"declarativeWebRequest", IDR_DECLARATIVE_WEBREQUEST_CUSTOM_BINDINGS_JS},
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

  for (const auto& resource : js_resources) {
    source_map->RegisterSource(resource.name, resource.id);
  }
}

void CoreExtensionsRendererAPIProvider::EnableCustomElementAllowlist() const {}

void CoreExtensionsRendererAPIProvider::RequireWebViewModules(
    ScriptContext* context) const {}

}  // namespace extensions
