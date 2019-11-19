// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "components/version_info/version_info.h"
#include "content/public/renderer/render_thread_observer.h"
#include "extensions/common/event_filter.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/user_script_set_manager.h"
#include "extensions/renderer/v8_schema_registry.h"
#include "third_party/blink/public/platform/web_string.h"
#include "v8/include/v8.h"

class ChromeRenderViewTest;
class GURL;
class ModuleSystem;
struct ExtensionMsg_DispatchEvent_Params;
struct ExtensionMsg_ExternalConnectionInfo;
struct ExtensionMsg_Loaded_Params;
struct ExtensionMsg_TabConnectionInfo;
struct ExtensionMsg_UpdatePermissions_Params;
struct ExtensionMsg_UpdateDefaultPolicyHostRestrictions_Params;

namespace blink {
class WebLocalFrame;
class WebServiceWorkerContextProxy;
}

namespace base {
class ListValue;
class SingleThreadTaskRunner;
}

namespace content {
class RenderThread;
}  // namespace content

namespace extensions {
class ContentWatcher;
class DispatcherDelegate;
class NativeExtensionBindingsSystem;
class IPCMessageSender;
class ScriptContext;
class ScriptContextSetIterable;
class ScriptInjectionManager;
class WorkerScriptContextSet;
struct EventFilteringInfo;
struct Message;
struct PortId;

// Dispatches extension control messages sent to the renderer and stores
// renderer extension related state.
class Dispatcher : public content::RenderThreadObserver,
                   public UserScriptSetManager::Observer {
 public:
  explicit Dispatcher(std::unique_ptr<DispatcherDelegate> delegate);
  ~Dispatcher() override;

  // Returns Service Worker ScriptContexts belonging to current worker thread.
  static WorkerScriptContextSet* GetWorkerScriptContextSet();

  const ScriptContextSet& script_context_set() const {
    return *script_context_set_;
  }

  // Returns iterator to iterate over all main thread ScriptContexts.
  ScriptContextSetIterable* script_context_set_iterator() {
    return script_context_set_.get();
  }

  V8SchemaRegistry* v8_schema_registry() { return v8_schema_registry_.get(); }

  const std::string& webview_partition_id() { return webview_partition_id_; }

  bool activity_logging_enabled() const { return activity_logging_enabled_; }

  void OnRenderThreadStarted(content::RenderThread* render_thread);

  void OnRenderFrameCreated(content::RenderFrame* render_frame);

  bool IsExtensionActive(const std::string& extension_id) const;

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
  // TODO(crbug.com/1000890): Figure out better way to coalesce them.
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
      const GURL& script_url);

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

  void OnExtensionResponse(int request_id,
                           bool success,
                           const base::ListValue& response,
                           const std::string& error);

  // Dispatches the event named |event_name| to all render views.
  void DispatchEvent(const std::string& extension_id,
                     const std::string& event_name,
                     const base::ListValue& event_args,
                     const EventFilteringInfo* filtering_info) const;

  // Shared implementation of the various MessageInvoke IPCs.
  void InvokeModuleSystemMethod(content::RenderFrame* render_frame,
                                const std::string& extension_id,
                                const std::string& module_name,
                                const std::string& function_name,
                                const base::ListValue& args);

  struct JsResourceInfo {
    const char* name = nullptr;
    int id = 0;
  };
  // Returns a list of resources for the JS modules to add to the source map.
  static std::vector<JsResourceInfo> GetJsResources();
  static void RegisterNativeHandlers(
      ModuleSystem* module_system,
      ScriptContext* context,
      Dispatcher* dispatcher,
      NativeExtensionBindingsSystem* bindings_system,
      V8SchemaRegistry* v8_schema_registry);

  NativeExtensionBindingsSystem* bindings_system() {
    return bindings_system_.get();
  }

 private:
  // The RendererPermissionsPolicyDelegateTest.CannotScriptWebstore test needs
  // to call the OnActivateExtension IPCs.
  friend class ::ChromeRenderViewTest;
  FRIEND_TEST_ALL_PREFIXES(RendererPermissionsPolicyDelegateTest,
                           CannotScriptWebstore);

  // RenderThreadObserver implementation:
  bool OnControlMessageReceived(const IPC::Message& message) override;

  void OnActivateExtension(const std::string& extension_id);
  void OnCancelSuspend(const std::string& extension_id);
  void OnDeliverMessage(int worker_thread_id,
                        const PortId& target_port_id,
                        const Message& message);
  void OnDispatchOnConnect(int worker_thread_id,
                           const PortId& target_port_id,
                           const std::string& channel_name,
                           const ExtensionMsg_TabConnectionInfo& source,
                           const ExtensionMsg_ExternalConnectionInfo& info);
  void OnDispatchOnDisconnect(int worker_thread_id,
                              const PortId& port_id,
                              const std::string& error_message);
  void OnLoaded(
      const std::vector<ExtensionMsg_Loaded_Params>& loaded_extensions);
  void OnMessageInvoke(const std::string& extension_id,
                       const std::string& module_name,
                       const std::string& function_name,
                       const base::ListValue& args);
  void OnDispatchEvent(const ExtensionMsg_DispatchEvent_Params& params,
                       const base::ListValue& event_args);
  void OnSetSessionInfo(version_info::Channel channel,
                        FeatureSessionType session_type,
                        bool lock_screen_context);
  void OnSetScriptingWhitelist(
      const ExtensionsClient::ScriptingWhitelist& extension_ids);
  void OnSetSystemFont(const std::string& font_family,
                       const std::string& font_size);
  void OnSetWebViewPartitionID(const std::string& partition_id);
  void OnShouldSuspend(const std::string& extension_id, uint64_t sequence_id);
  void OnSuspend(const std::string& extension_id);
  void OnTransferBlobs(const std::vector<std::string>& blob_uuids);
  void OnUnloaded(const std::string& id);
  void OnUpdatePermissions(const ExtensionMsg_UpdatePermissions_Params& params);
  void OnUpdateDefaultPolicyHostRestrictions(
      const ExtensionMsg_UpdateDefaultPolicyHostRestrictions_Params& params);
  void OnUpdateTabSpecificPermissions(const GURL& visible_url,
                                      const std::string& extension_id,
                                      const URLPatternSet& new_hosts,
                                      bool update_origin_whitelist,
                                      int tab_id);
  void OnClearTabSpecificPermissions(
      const std::vector<std::string>& extension_ids,
      bool update_origin_whitelist,
      int tab_id);

  void OnSetActivityLoggingEnabled(bool enabled);

  // UserScriptSetManager::Observer implementation.
  void OnUserScriptsUpdated(const std::set<HostID>& changed_hosts) override;

  void UpdateActiveExtensions();

  // Sets up the host permissions for |extension|.
  void InitOriginPermissions(const Extension* extension);

  // Updates the host permissions for the extension url to include only those
  // the extension currently has, removing any old entries.
  void UpdateOriginPermissions(const Extension& extension);

  // Enable custom element whitelist in Apps.
  void EnableCustomElementWhiteList();

  // Adds or removes bindings for all contexts.
  void UpdateAllBindings();

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
      std::unique_ptr<IPCMessageSender> ipc_sender);

  // The delegate for this dispatcher to handle embedder-specific logic.
  std::unique_ptr<DispatcherDelegate> delegate_;

  // The IDs of extensions that failed to load, mapped to the error message
  // generated on failure.
  std::map<std::string, std::string> extension_load_errors_;

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
  ScopedObserver<UserScriptSetManager, UserScriptSetManager::Observer>
      user_script_set_manager_observer_;

  // Whether or not extension activity is enabled.
  bool activity_logging_enabled_;

  // The WebView partition ID associated with this process's storage partition,
  // if this renderer is a WebView guest render process. Otherwise, this will be
  // empty.
  std::string webview_partition_id_;

  // Used to hold a service worker information which is ready to execute but the
  // onloaded message haven't been received yet. We need to defer service worker
  // execution until the ExtensionMsg_Loaded message is received because we can
  // install extension bindings only after the onload message is received.
  // TODO(bashi): Consider to have a separate class to put this logic?
  struct PendingServiceWorker {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    blink::WebServiceWorkerContextProxy* context_proxy;

    PendingServiceWorker(blink::WebServiceWorkerContextProxy* context_proxy);
    ~PendingServiceWorker();
  };
  // This will be accessed both from the main thread and worker threads.
  std::map<ExtensionId, std::unique_ptr<PendingServiceWorker>>
      service_workers_paused_for_on_loaded_message_;
  base::Lock service_workers_paused_for_on_loaded_message_lock_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DISPATCHER_H_
