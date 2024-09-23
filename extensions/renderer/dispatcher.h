// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DISPATCHER_H_
#define EXTENSIONS_RENDERER_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/version_info/channel.h"
#include "content/public/renderer/render_thread_observer.h"
#include "extensions/common/event_filter.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/host_id.mojom-forward.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/user_script_set_manager.h"
#include "extensions/renderer/v8_schema_registry.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_string.h"
#include "v8/include/v8-forward.h"

class ChromeRenderViewTest;
class GURL;

namespace blink {
class WebLocalFrame;
class WebServiceWorkerContextProxy;
}

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
class RenderThread;
}  // namespace content

namespace extensions {

// Constant to define the default profile id for the renderer to 0.
// Since each renderer is associated with a single context, we don't need
// separate ids for the profile.
const int kRendererProfileId = 0;

class ContentWatcher;
class Extension;
class ExtensionsRendererAPIProvider;
class ModuleSystem;
class IPCMessageSender;
class ScriptContext;
class ScriptContextSetIterable;
class ScriptInjectionManager;
class WorkerScriptContextSet;

// Dispatches extension control messages sent to the renderer and stores
// renderer extension related state.
class Dispatcher : public content::RenderThreadObserver,
                   public UserScriptSetManager::Observer,
                   public mojom::Renderer,
                   public mojom::EventDispatcher,
                   public NativeExtensionBindingsSystem::Delegate {
 public:
  explicit Dispatcher(
      std::vector<std::unique_ptr<const ExtensionsRendererAPIProvider>>
          api_providers);

  Dispatcher(const Dispatcher&) = delete;
  Dispatcher& operator=(const Dispatcher&) = delete;

  ~Dispatcher() override;

  // Returns Service Worker ScriptContexts belonging to current worker thread.
  static WorkerScriptContextSet* GetWorkerScriptContextSet();

  // Returns true if web socket activity for the service worker associated with
  // the given `v8_context` should count as service worker activity, prolonging
  // the service worker's lifetime.
  // Called on the service worker thread.
  static bool ShouldNotifyServiceWorkerOnWebSocketActivity(
      v8::Local<v8::Context> v8_context);

  const ScriptContextSet& script_context_set() const {
    return *script_context_set_;
  }

  // Returns iterator to iterate over all main thread ScriptContexts.
  ScriptContextSetIterable* script_context_set_iterator() {
    return script_context_set_.get();
  }

  V8SchemaRegistry* v8_schema_registry() { return v8_schema_registry_.get(); }

  const std::optional<std::string>& webview_partition_id() {
    return webview_partition_id_;
  }

  bool activity_logging_enabled() const { return activity_logging_enabled_; }

  void OnRenderThreadStarted(content::RenderThread* render_thread);

  void OnRenderFrameCreated(content::RenderFrame* render_frame);

  bool IsExtensionActive(const ExtensionId& extension_id) const;

  void DidCreateScriptContext(blink::WebLocalFrame* frame,
                              const v8::Local<v8::Context>& context,
                              int32_t world_id);

  // This is called when a service worker is ready to evaluate the toplevel
  // script. This method suspends the service worker if:
  // * the service worker is background of a service worker based extension,
  // and
  // * the extension isn't loaded yet.
  // Suspending background service worker is required because we need to
  // install extension API bindings before executing the service worker.
  // TODO(crbug.com/40645846): Figure out better way to coalesce them.
  //
  // Runs on the service worker thread and should only use thread-safe member
  // variables.
  void DidInitializeServiceWorkerContextOnWorkerThread(
      blink::WebServiceWorkerContextProxy* context_proxy,
      const GURL& service_worker_scope,
      const GURL& script_url);

  // This is called immediately before a service worker evaluates the
  // toplevel script. This method installs extension API bindings.
  //
  // Runs on a different thread and should only use thread-safe member
  // variables.
  void WillEvaluateServiceWorkerOnWorkerThread(
      blink::WebServiceWorkerContextProxy* context_proxy,
      v8::Local<v8::Context> v8_context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url,
      const blink::ServiceWorkerToken& service_worker_token);

  void WillReleaseScriptContext(blink::WebLocalFrame* frame,
                                const v8::Local<v8::Context>& context,
                                int32_t world_id);

  // Runs on worker thread and should not use any member variables.
  void DidStartServiceWorkerContextOnWorkerThread(
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url);

  // Runs on a different thread and should not use any member variables.
  void WillDestroyServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> v8_context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url);

  // This method is not allowed to run JavaScript code in the frame.
  void DidCreateDocumentElement(blink::WebLocalFrame* frame);

  // These methods may run (untrusted) JavaScript code in the frame, and
  // cause |render_frame| to become invalid.
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame);

  // Dispatches the event named |event_name| to all render views.
  void DispatchEventHelper(const mojom::HostID& extension_id,
                           const std::string& event_name,
                           const base::Value::List& event_args,
                           mojom::EventFilteringInfoPtr filtering_info) const;

  // Shared implementation of the various MessageInvoke IPCs.
  void InvokeModuleSystemMethod(content::RenderFrame* render_frame,
                                const ExtensionId& extension_id,
                                const std::string& module_name,
                                const std::string& function_name,
                                const base::Value::List& args);

  void ExecuteDeclarativeScript(content::RenderFrame* render_frame,
                                int tab_id,
                                const ExtensionId& extension_id,
                                const std::string& script_id,
                                const GURL& url);

  // Executes the code described in |param| and calls |callback| if it's done.
  void ExecuteCode(mojom::ExecuteCodeParamsPtr param,
                   mojom::LocalFrame::ExecuteCodeCallback callback,
                   content::RenderFrame* render_frame);

  NativeExtensionBindingsSystem* bindings_system() {
    return bindings_system_.get();
  }

 private:
  // The RendererPermissionsPolicyDelegateTest.CannotScriptWebstore test needs
  // to call the ActivateExtension IPCs.
  friend class ::ChromeRenderViewTest;
  FRIEND_TEST_ALL_PREFIXES(RendererPermissionsPolicyDelegateTest,
                           CannotScriptWebstore);

  // RenderThreadObserver implementation:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // mojom::Renderer implementation:
  void ActivateExtension(const ExtensionId& extension_id) override;
  void SetActivityLoggingEnabled(bool enabled) override;
  void LoadExtensions(
      std::vector<mojom::ExtensionLoadedParamsPtr> loaded_extensions) override;
  void UnloadExtension(const ExtensionId& extension_id) override;
  void SuspendExtension(
      const ExtensionId& extension_id,
      mojom::Renderer::SuspendExtensionCallback callback) override;
  void CancelSuspendExtension(const ExtensionId& extension_id) override;
  void SetDeveloperMode(bool current_developer_mode) override;
  void SetSessionInfo(version_info::Channel channel,
                      mojom::FeatureSessionType session_type,
                      bool lock_screen_context) override;
  void SetSystemFont(const std::string& font_family,
                     const std::string& font_size) override;
  void SetWebViewPartitionID(const std::string& partition_id) override;
  void SetScriptingAllowlist(
      const std::vector<ExtensionId>& extension_ids) override;
  void UpdateUserScriptWorlds(
      std::vector<mojom::UserScriptWorldInfoPtr> infos) override;
  void ClearUserScriptWorldConfig(
      const ExtensionId& extension_id,
      const std::optional<std::string>& world_id) override;
  void ShouldSuspend(ShouldSuspendCallback callback) override;
  void TransferBlobs(TransferBlobsCallback callback) override;
  void UpdatePermissions(const ExtensionId& extension_id,
                         PermissionSet active_permissions,
                         PermissionSet withheld_permissions,
                         URLPatternSet policy_blocked_hosts,
                         URLPatternSet policy_allowed_hosts,
                         bool uses_default_policy_host_restrictions) override;
  void UpdateDefaultPolicyHostRestrictions(
      URLPatternSet default_policy_blocked_hosts,
      URLPatternSet default_policy_allowed_hosts) override;
  void UpdateUserHostRestrictions(URLPatternSet user_blocked_hosts,
                                  URLPatternSet user_allowed_hosts) override;
  void UpdateTabSpecificPermissions(const ExtensionId& extension_id,
                                    URLPatternSet new_hosts,
                                    int tab_id,
                                    bool update_origin_allowlist) override;
  void UpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory,
                         mojom::HostIDPtr host_id) override;
  void ClearTabSpecificPermissions(
      const std::vector<ExtensionId>& extension_ids,
      int tab_id,
      bool update_origin_allowlist) override;
  void WatchPages(const std::vector<std::string>& css_selectors) override;

  void OnRendererAssociatedRequest(
      mojo::PendingAssociatedReceiver<mojom::Renderer> receiver);
  void OnEventDispatcherRequest(
      mojo::PendingAssociatedReceiver<mojom::EventDispatcher> receiver);

  // mojom::EventDispatcher implementation.
  void DispatchEvent(mojom::DispatchEventParamsPtr params,
                     base::Value::List event_args,
                     DispatchEventCallback callback) override;

  // UserScriptSetManager::Observer implementation.
  void OnUserScriptsUpdated(const mojom::HostID& changed_host) override;

  // NativeExtensionBindingsSystem::Delegate implementation.
  ScriptContextSetIterable* GetScriptContextSet() override;

  void UpdateActiveExtensions();

  // Sets up the host permissions for |extension|.
  void InitOriginPermissions(const Extension* extension);

  // Updates the host permissions for the extension url to include only those
  // the extension currently has, removing any old entries.
  void UpdateOriginPermissions(const Extension& extension);

  // Enable custom element allowlist in Apps.
  void EnableCustomElementAllowlist();

  // Adds or removes bindings for all contexts. `api_permissions_changed`
  // indicates whether the effective permission state for extensions has
  // changed and cached features should be re-calculated.
  void UpdateAllBindings(bool api_permissions_changed);

  // Adds or removes bindings for every context belonging to |extension|, due to
  // permissions change in the extension.
  void UpdateBindingsForExtension(const Extension& extension);

  void RegisterNativeHandlers(ModuleSystem* module_system,
                              ScriptContext* context,
                              NativeExtensionBindingsSystem* bindings_system,
                              V8SchemaRegistry* v8_schema_registry);

  // Inserts static source code into |source_map_|.
  void PopulateSourceMap();

  // Returns whether the current renderer hosts a platform app.
  bool IsWithinPlatformApp();

  // Requires the GuestView modules in the module system of the ScriptContext
  // |context|.
  void RequireGuestViewModules(ScriptContext* context);

  // Creates the NativeExtensionBindingsSystem. Note: this may be called on any
  // thread, and thus cannot mutate any state or rely on state which can be
  // mutated in Dispatcher.
  std::unique_ptr<NativeExtensionBindingsSystem> CreateBindingsSystem(
      NativeExtensionBindingsSystem::Delegate* delegate,
      std::unique_ptr<IPCMessageSender> ipc_sender);

  void ResumeEvaluationOnWorkerThread(const ExtensionId& extension_id);

  // The list of embedder API providers.
  // This list is accessed on multiple threads, since these API providers are
  // used in the initialization of script contexts (which can be both main-
  // thread contexts and worker-thread contexts).
  // This is safe, since this list is established on Dispatcher construction
  // (which happens before any access on worker threads), the Dispatcher should
  // not be destroyed, and this list is immutable. This is enforced by the
  // `const`s below.
  const std::vector<std::unique_ptr<const ExtensionsRendererAPIProvider>>
      api_providers_;

  // The IDs of extensions that failed to load, mapped to the error message
  // generated on failure.
  std::map<ExtensionId, std::string> extension_load_errors_;

  // ExtensionIds for extensions that were loaded, but then unloaded later.
  // Used for metrics purposes.
  std::set<ExtensionId> unloaded_extensions_;

  // All the bindings contexts that are currently loaded for this renderer.
  // There is zero or one for each v8 context.
  std::unique_ptr<ScriptContextSet> script_context_set_;

  std::unique_ptr<ContentWatcher> content_watcher_;

  std::unique_ptr<UserScriptSetManager> user_script_set_manager_;

  std::unique_ptr<ScriptInjectionManager> script_injection_manager_;

  // The extensions and apps that are active in this process.
  ExtensionIdSet active_extension_ids_;

  ResourceBundleSourceMap source_map_;

  // Cache for the v8 representation of extension API schemas.
  std::unique_ptr<V8SchemaRegistry> v8_schema_registry_;

  // The bindings system associated with the main thread.
  std::unique_ptr<NativeExtensionBindingsSystem> bindings_system_;

  // The platforms system font family and size;
  std::string system_font_family_;
  std::string system_font_size_;

  // It is important for this to come after the ScriptInjectionManager, so that
  // the observer is destroyed before the UserScriptSet.
  base::ScopedObservation<UserScriptSetManager, UserScriptSetManager::Observer>
      user_script_set_manager_observation_{this};

  // Whether or not extension activity is enabled.
  bool activity_logging_enabled_;

  // The WebView partition ID associated with this process's storage partition,
  // if this renderer is a WebView guest render process, otherwise unset.
  // Note that this may be an empty string, even if it's set (if the webview
  // doesn't have a set partition ID).
  std::optional<std::string> webview_partition_id_;

  // Extensions renderer receiver. This is an associated receiver because
  // it is dependent on other messages sent on other associated channels.
  mojo::AssociatedReceiver<mojom::Renderer> receiver_;

  // Extensions mojom::EventDispatcher receiver. This is an associated receiver
  // because it is dependent on other messages sent on other associated
  // channels.
  mojo::AssociatedReceiver<mojom::EventDispatcher> dispatcher_;

  // Used to hold a service worker information which is ready to execute but the
  // onloaded message haven't been received yet. We need to defer service worker
  // execution until the ExtensionMsg_Loaded message is received because we can
  // install extension bindings only after the onload message is received.
  // TODO(bashi): Consider to have a separate class to put this logic?
  struct PendingServiceWorker {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    raw_ptr<blink::WebServiceWorkerContextProxy> context_proxy;

    PendingServiceWorker(blink::WebServiceWorkerContextProxy* context_proxy);
    ~PendingServiceWorker();
  };
  // This will be accessed both from the main thread and worker threads.
  std::map<ExtensionId, std::unique_ptr<PendingServiceWorker>>
      service_workers_paused_for_on_loaded_message_;
  base::Lock service_workers_paused_for_on_loaded_message_lock_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DISPATCHER_H_
