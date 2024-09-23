// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_API_EVENT_HANDLER_H_
#define EXTENSIONS_RENDERER_BINDINGS_API_EVENT_HANDLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_event_listeners.h"
#include "extensions/renderer/bindings/event_emitter.h"
#include "extensions/renderer/bindings/listener_tracker.h"
#include "v8/include/v8.h"

namespace extensions {
class APIResponseValidator;
class ExceptionHandler;

// The object to handle API events. This includes vending v8::Objects for the
// event; handling adding, removing, and querying listeners; and firing events
// to subscribed listeners. Designed to be used across JS contexts, but on a
// single thread.
class APIEventHandler {
 public:
  // A callback to retrieve the owner of the context's identity. This allows us
  // to associate multiple listeners from different v8::Contexts with the same
  // owner (e.g., extension).
  using ContextOwnerIdGetter =
      base::RepeatingCallback<std::string(v8::Local<v8::Context>)>;

  APIEventHandler(const APIEventListeners::ListenersUpdated& listeners_changed,
                  const ContextOwnerIdGetter& context_owner_id_getter,
                  ExceptionHandler* exception_handler);

  APIEventHandler(const APIEventHandler&) = delete;
  APIEventHandler& operator=(const APIEventHandler&) = delete;

  ~APIEventHandler();

  // Sets the response validator to be used in verifying event arguments.
  void SetResponseValidator(std::unique_ptr<APIResponseValidator> validator);

  // Returns a new v8::Object for an event with the given |event_name|. If
  // |notify_on_change| is true, notifies whenever listeners state is changed.
  // TODO(devlin): Maybe worth creating a Params struct to hold the event
  // information?
  v8::Local<v8::Object> CreateEventInstance(const std::string& event_name,
                                            bool supports_filters,
                                            bool supports_lazy_listeners,
                                            int max_listeners,
                                            bool notify_on_change,
                                            v8::Local<v8::Context> context);

  // Creates a new event without any name. This is used by custom bindings when
  // the entirety of the logic for the event is contained in the renderer. These
  // events do not notify of new/removed listeners or allow for dispatching
  // through FireEventInContext().
  v8::Local<v8::Object> CreateAnonymousEventInstance(
      v8::Local<v8::Context> context);

  // Invalidates the given |event|.
  void InvalidateCustomEvent(v8::Local<v8::Context> context,
                             v8::Local<v8::Object> event);

  // Notifies all listeners of the event with the given |event_name| in the
  // specified |context|, sending the included |arguments|.
  // Warning: This runs arbitrary JS code, so the |context| may be invalidated
  // after this!
  void FireEventInContext(const std::string& event_name,
                          v8::Local<v8::Context> context,
                          const base::Value::List& arguments,
                          mojom::EventFilteringInfoPtr filter);
  void FireEventInContext(const std::string& event_name,
                          v8::Local<v8::Context> context,
                          v8::LocalVector<v8::Value>* arguments,
                          mojom::EventFilteringInfoPtr filter,
                          JSRunner::ResultCallback callback);

  // Registers a |function| to serve as an "argument massager" for the given
  // |event_name|, mutating the original arguments.
  // The function is called with two arguments: the array of original arguments
  // being dispatched to the event, and the function to dispatch the event to
  // listeners.
  void RegisterArgumentMassager(v8::Local<v8::Context> context,
                                const std::string& event_name,
                                v8::Local<v8::Function> function);

  // Returns true if there is a listener for the given |event_name| in the
  // given |context|.
  bool HasListenerForEvent(const std::string& event_name,
                           v8::Local<v8::Context> context);

  // Invalidates listeners for the given |context|. It's a shame we have to
  // have this separately (as opposed to hooking into e.g. a PerContextData
  // destructor), but we need to do this before the context is fully removed
  // (because the associated extension ScriptContext needs to be valid).
  void InvalidateContext(v8::Local<v8::Context> context);

  // Returns the number of event listeners for a given |event_name| and
  // |context|.
  size_t GetNumEventListenersForTesting(const std::string& event_name,
                                        v8::Local<v8::Context> context);

 private:
  APIEventListeners::ListenersUpdated listeners_changed_;

  ContextOwnerIdGetter context_owner_id_getter_;

  // The shared ListenerTracker for all listeners in the system.
  ListenerTracker listener_tracker_;

  // The exception handler associated with the bindings system; guaranteed to
  // outlive this object.
  const raw_ptr<ExceptionHandler> exception_handler_;

  // The response validator used to verify event arguments. Only non-null if
  // validation is enabled.
  std::unique_ptr<APIResponseValidator> api_response_validator_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_API_EVENT_HANDLER_H_
