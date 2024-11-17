// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_
#define EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"
#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/bindings/event_emitter.h"
#include "extensions/renderer/feature_cache.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace extensions {
class IPCMessageSender;
class ScriptContext;
class ScriptContextSetIterable;

// The class responsible for creating extension bindings in different contexts,
// permissions/availability checks, dispatching requests and handling responses,
// and dispatching events to listeners.
// This is designed to be used on a single thread (and for all contexts on that
// thread), but should be safe to use on threads other than the main thread (so
// that worker threads can have extension bindings).
// TODO(devlin): Rename this to be simply "ExtensionBindingsSystem"? There's
// no non-native version, but the rename causes churn and also makes git history
// a bit messy (since there used to be a different ExtensionBindingsSystem).
class NativeExtensionBindingsSystem {
 public:
  class Delegate {
   public:
    virtual ScriptContextSetIterable* GetScriptContextSet() = 0;
    virtual ~Delegate() = default;
  };

  explicit NativeExtensionBindingsSystem(
      Delegate* delegate,
      std::unique_ptr<IPCMessageSender> ipc_message_sender);

  NativeExtensionBindingsSystem(const NativeExtensionBindingsSystem&) = delete;
  NativeExtensionBindingsSystem& operator=(
      const NativeExtensionBindingsSystem&) = delete;

  ~NativeExtensionBindingsSystem();

  // Called when a new ScriptContext is created.
  // Initializes the bindings for a newly created |context|.
  void DidCreateScriptContext(ScriptContext* context);

  // Called when a ScriptContext is about to be released.
  void WillReleaseScriptContext(ScriptContext* context);

  // Updates the bindings for a given |context|. This happens at initialization,
  // but also when e.g. an extension gets updated permissions.
  // TODO(lazyboy): Make this private, and expose a test getter.
  void UpdateBindingsForContext(ScriptContext* context);

  // Dispatches an event with the given |name|, |event_args|, and
  // |filtering_info| in the given |context|.
  void DispatchEventInContext(
      const std::string& event_name,
      const base::Value::List& event_args,
      const mojom::EventFilteringInfoPtr& filtering_info,
      ScriptContext* context);

  // Returns true if there is a listener for the given |event_name| in the
  // associated |context|.
  bool HasEventListenerInContext(const std::string& event_name,
                                 ScriptContext* context);

  // Handles the response associated with the given |request_id|.
  void HandleResponse(int request_id,
                      bool success,
                      const base::Value::List& response,
                      const std::string& error,
                      mojom::ExtraResponseDataPtr extra_data = nullptr);

  // Returns the associated IPC message sender.
  IPCMessageSender* GetIPCMessageSender();

  // Adds or removes bindings for every context belonging to |extension_id|, or
  // or all contexts if |extension_id| is empty. Also invalidates
  // |feature_cache_| entry if |permissions_changed| = true.
  void UpdateBindings(const ExtensionId& extension_id,
                      bool permissions_changed,
                      ScriptContextSetIterable* script_context_set);

  // Called when an extension is removed.
  void OnExtensionRemoved(const ExtensionId& id);

  APIBindingsSystem* api_system() { return &api_system_; }
  NativeRendererMessagingService* messaging_service() {
    return &messaging_service_;
  }
  Delegate* delegate() { return delegate_; }

  // Returns the API with the given |name| for the given |context|. Used for
  // testing purposes.
  v8::Local<v8::Object> GetAPIObjectForTesting(ScriptContext* context,
                                               const std::string& api_name);

 private:
  // Handles sending a given |request|, forwarding it on to the send_ipc_ after
  // adding additional info.
  void SendRequest(std::unique_ptr<APIRequestHandler::Request> request,
                   v8::Local<v8::Context> context);

  // Called when listeners for a given event have changed, and forwards it along
  // to |send_event_listener_ipc_|.
  void OnEventListenerChanged(const std::string& event_name,
                              binding::EventListenersChanged change,
                              const base::Value::Dict* filter,
                              bool was_manual,
                              v8::Local<v8::Context> context);

  // Getter callback for an extension API, since APIs are constructed lazily.
  static void BindingAccessor(v8::Local<v8::Name> name,
                              const v8::PropertyCallbackInfo<v8::Value>& info);

  // Callback for accessing a restricted extension API. Access to the API is
  // restricted to the developer mode only.
  static void ThrowDeveloperModeRestrictedError(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);

  // Creates and returns the API binding for the given |name|.
  static v8::Local<v8::Object> GetAPIHelper(v8::Local<v8::Context> context,
                                            v8::Local<v8::String> name);

  // Gets the chrome.runtime API binding.
  static v8::Local<v8::Object> GetLastErrorParents(
      v8::Local<v8::Context> context,
      v8::Local<v8::Object>* secondary_parent);

  // Callback to get an API binding for an internal API.
  static void GetInternalAPI(const v8::FunctionCallbackInfo<v8::Value>& info);

  // Helper method to get a APIBindingJSUtil object for the current context,
  // and populate |binding_util_out|. We use an out parameter instead of
  // returning it in order to let us use weak ptrs, which can't be used on a
  // method with a return value.
  void GetJSBindingUtil(v8::Local<v8::Context> context,
                        v8::Local<v8::Value>* binding_util_out);

  // Updates a web page context within |context| with any content capabilities
  // granted by active extensions.
  void UpdateContentCapabilities(ScriptContext* context);

  // Creates the parameters objects inside chrome.scripting, if |context| is for
  // content scripts running in an isolated world.
  void SetScriptingParams(ScriptContext* context);

  // Updates the bindings to expose the prompt API available for extensions.
  void UpdateBindingsForPromptAPI(ScriptContext* context);

  // Remove the prompt API bindings if none of the other feature flags are
  // enabled.
  void MaybeRemoveUnnecessaryPromptAPIBinding(ScriptContext* context);

  const raw_ptr<Delegate> delegate_;

  std::unique_ptr<IPCMessageSender> ipc_message_sender_;

  // The APIBindingsSystem associated with this class.
  APIBindingsSystem api_system_;

  NativeRendererMessagingService messaging_service_;

  FeatureCache feature_cache_;

  // A function to acquire an internal API.
  v8::Eternal<v8::FunctionTemplate> get_internal_api_;

  base::WeakPtrFactory<NativeExtensionBindingsSystem> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_
