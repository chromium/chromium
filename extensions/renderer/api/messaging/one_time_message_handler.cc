// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/one_time_message_handler.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/supports_user_data.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/renderer/api/messaging/message_target.h"
#include "extensions/renderer/api/messaging/messaging_util.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/get_per_context_data.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/gc_callback.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "gin/arguments.h"
#include "gin/data_object_builder.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

// An opener port in the context; i.e., the caller of runtime.sendMessage.
struct OneTimeOpener {
  int request_id = -1;
  binding::AsyncResponseType async_type = binding::AsyncResponseType::kNone;
  mojom::ChannelType channel_type;
};

// A receiver port in the context; i.e., a listener to runtime.onMessage.
struct OneTimeReceiver {
  std::string event_name;
  v8::Global<v8::Object> sender;
  v8::Global<v8::Function> message_response_function;
};

struct OneTimeMessageContextData : public base::SupportsUserData::Data {
  static constexpr char kPerContextDataKey[] =
      "extension_one_time_message_context_data";

  std::map<PortId, OneTimeOpener> openers;
  // If the receiver is still present for `PortId`, then a response can still be
  // sent from the listener to the message sender. Otherwise no response (or
  // error) should be sent back to the message sender from the listener.
  std::map<PortId, OneTimeReceiver> receivers;

  using OneTimePortCallbacks =
      std::map<OneTimeMessageHandler::CallbackID,
               std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback>>;

  // Owns the pending callbacks used for message replies. A listener's v8
  // context may invoke a response callback asynchronously. This map keeps the
  // callback alive until it is invoked or the connection is closed. Note: this
  // struct is accessed by `OneTimeMessageHandler` but this collection is
  // conceptually owned by
  // `OneTimeMessageHandler::OneTimeMessageCallbackManager`. It is placed in
  // this struct for simplicity since the classes are so interrelated.
  std::map<PortId, OneTimePortCallbacks> pending_receiver_callbacks;
};

constexpr char OneTimeMessageContextData::kPerContextDataKey[];

bool IsMessagePolyfillSupportEnabled() {
  return base::FeatureList::IsEnabled(
      extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport);
}

// Returns an array from the `result` object's `property_name` if it exists,
// otherwise returns an empty `v8::Local<v8::Array>`.
v8::Local<v8::Array> GetListenerResultArray(v8::Isolate* isolate,
                                            v8::Local<v8::Context> context,
                                            v8::Local<v8::Value> result,
                                            const char* property_name) {
  // `result` can be undefined if the context was destroyed before the
  // listeners were run (or while they were running).
  if (result->IsUndefined()) {
    return v8::Local<v8::Array>();
  }

  // We expect results as a value with an array of results as a `property_name`
  // property, however, since this comes from untrusted JS let's confirm this
  // first.
  if (!result->IsObject()) {
    return v8::Local<v8::Array>();
  }
  v8::Local<v8::Object> result_object = result.As<v8::Object>();
  v8::Local<v8::Value> array_value;
  if (!result_object->Get(context, gin::StringToSymbol(isolate, property_name))
           .ToLocal(&array_value) ||
      !array_value->IsArray()) {
    return v8::Local<v8::Array>();
  }

  return array_value.As<v8::Array>();
}

void DelayedOneTimeMessageCallbackHelper(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(info.Data()->IsString());

  gin::Arguments arguments(info);
  v8::Isolate* isolate = arguments.isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(context,
                                                   kDontCreateIfMissing);
  if (!data)
    return;

  // Retrieve the CallbackID from v8 that we set when we created the callback.
  v8::Local<v8::String> callback_id_v8_string = info.Data().As<v8::String>();
  std::string callback_id_string;
  if (!gin::Converter<std::string>::FromV8(isolate, callback_id_v8_string,
                                           &callback_id_string)) {
    return;
  }
  std::optional<OneTimeMessageHandler::CallbackID> callback_id =
      OneTimeMessageHandler::CallbackID::DeserializeFromString(
          callback_id_string);
  if (!callback_id) {
    // Something must've changed this value unexpectedly. In any case we don't
    // know which callback to run so don't run any callbacks. If this was
    // intended for a pending callback that is still in
    // `data->pending_receiver_callbacks`, it will be garbage collected later in
    // `OnDelayedOneTimeMessageCallbackCollected()`.
    return;
  }

  // Search each `PortId` in `data->pending_receiver_callbacks` to see if any of
  // them have `callback_id`.
  OneTimeMessageContextData::OneTimePortCallbacks* port_callbacks = nullptr;
  OneTimeMessageContextData::OneTimePortCallbacks::iterator port_callback_iter;
  for (auto& port_entry : data->pending_receiver_callbacks) {
    auto callback_entry = port_entry.second.find(*callback_id);
    if (callback_entry == port_entry.second.end()) {
      // `callback_id` is not associated with this `PortId`.
      continue;
    }
    // Found the callback for this `callback_id`. There shouldn't be any
    // duplicates so stop searching.
    port_callbacks = &port_entry.second;
    port_callback_iter = callback_entry;
    break;
  }

  // Couldn't find `callback_id` amongst the `PortId`s for this extension.
  if (!port_callbacks) {
    // One way this can happen is if an extension attempts to respond to a
    // message multiple times despite us only allowing the first response to
    // be sent back to the sender. If that happens, just return early to
    // enforce this.
    return;
  }

  std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback> callback =
      std::move(port_callback_iter->second);
  port_callbacks->erase(port_callback_iter);
  std::move(*callback).Run(&arguments);
}

}  // namespace

// A helper class to manage the creation and tracking of callbacks for
// one-time messages, such as the message response callback.
//
// This class creates `v8::Function`s that are associated to a C++ callback
// (`OneTimeMessageCallback`) and handles the cleanup of associated resources
// when the `v8::Function` is garbage collected. This allows message listeners
// to reply asynchronously without leaking resources.
//
// An instance of this class is held by the `OneTimeMessageHandler` and its
// lifetime is tied to the handler.
class OneTimeMessageHandler::OneTimeMessageCallbackManager {
 public:
  explicit OneTimeMessageCallbackManager(
      OneTimeMessageHandler& owning_message_handler);
  ~OneTimeMessageCallbackManager();

  OneTimeMessageCallbackManager(const OneTimeMessageCallbackManager&) = delete;
  OneTimeMessageCallbackManager& operator=(
      const OneTimeMessageCallbackManager&) = delete;

  // Returns a v8 function that will call `callback` when the the function is
  // called in v8. `callback` will be cleaned up when the returned function is
  // garbage collected by v8.
  v8::Local<v8::Function> CreateRespondingFunction(
      ScriptContext& script_context,
      const PortId& port_id,
      std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback> callback);

  // Returns a v8 function that will call `callback` after the sender's
  // message is dispatched to all message listeners. `callback` will *not* be
  // cleaned up when the returned function is garbage collected by v8 since it
  // is expected to always be called synchronously immediately after event
  // dispatch.
  v8::Local<v8::Function> CreateEventDispatchFunction(
      ScriptContext& script_context,
      const PortId& port_id,
      std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback> callback);

  // Returns a v8 function that will call `callback` whenever a listener in the
  // receiver throws a synchronous error. `callback` will *not* be cleaned up
  // when the returned function is garbage collected by v8 since it is expected
  // to cleaned up by either a) being called, or b) being deleted after all
  // listeners have been dispatched to.
  // `callback_id` is the unique ID of `callback` for later retrieval when the
  // returned function calls `callback.`
  v8::Local<v8::Function> CreateListenerThrowsErrorFunction(
      ScriptContext& script_context,
      const PortId& port_id,
      std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback> callback,
      const CallbackID& callback_id);

  // Clears any pending `OneTimeMessageHandler::OneTimeMessageCallback`s that
  // could be called for `port_id`.
  void ClearCallbackDataForPortId(ScriptContext* script_context,
                                  const PortId& port_id);

  // Deletes `port_id`'s `OneTimeMessageHandler::OneTimeMessageCallback` that is
  // identified by `callback_id` .
  void DeleteCallbackDataForCallbackId(
      ScriptContext* script_context,
      const PortId& port_id,
      const OneTimeMessageHandler::CallbackID& callback_id);

  // Gets the number of pending callbacks for the `port_id` on the associated
  // per context data for testing purposes.
  int GetPendingCallbackCountForTest(ScriptContext* script_context,  // IN-TEST
                                     const PortId& port_id);

 private:
  // Helper method for creating delayed callbacks that can be called as a
  // result of message listener behavior. `cleanup_if_function_unused` true
  // means that, if the context is still valid when the `v8::Function` that is
  // created to call `callback` is garbage collected, we'll cleanup
  // `callback`.
  // If polyfill support is enabled we'll notify `OneTimeMessageHandler` if
  // there are no more `OneTimeMessageHandler::OneTimeMessageCallback`s to
  // collect, otherwise we'll notify of the one and only callback (message
  // response) collection.
  // `optional_callback_id` allows the caller to specify the
  // `OneTimeMessageHandler::CallbackID` used to identify the callback,
  // otherwise one will be generated for them.
  v8::Local<v8::Function> CreateDelayedOneTimeMessageCallback(
      ScriptContext& script_context,
      const PortId& port_id,
      std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback> callback,
      bool cleanup_if_function_unused);
  v8::Local<v8::Function> CreateDelayedOneTimeMessageCallback(
      ScriptContext& script_context,
      const PortId& port_id,
      std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback> callback,
      bool cleanup_if_function_unused,
      std::optional<CallbackID> optional_callback_id);

  // Triggered when a `v8::Function` that had `cleanup_if_function_unused` set
  // to true when it was created is no longer accessible in the context and v8
  // has garbage collected it.
  // Used to clean up data that was stored for the `v8::Function` (the
  // `OneTimeMessageHandler::OneTimeMessageCallback` it is associated with)
  // and for closing the associated message port. `callback_id` is the ID of
  // the associated `OneTimeMessageHandler::OneTimeMessageCallback`, needed
  // for finding and erasing it from the OneTimeMessageContextData.
  void OnDelayedOneTimeMessageCallbackCollected(
      ScriptContext* script_context,
      const PortId& port_id,
      OneTimeMessageHandler::CallbackID callback_id);

  // The owning OneTimeMessageHandler. Outlives this object.
  const raw_ref<OneTimeMessageHandler> message_handler_;

  base::WeakPtrFactory<OneTimeMessageHandler::OneTimeMessageCallbackManager>
      weak_factory_{this};
};

OneTimeMessageHandler::OneTimeMessageCallbackManager::
    OneTimeMessageCallbackManager(OneTimeMessageHandler& owning_message_handler)
    : message_handler_(owning_message_handler) {}
OneTimeMessageHandler::OneTimeMessageCallbackManager::
    ~OneTimeMessageCallbackManager() = default;

v8::Local<v8::Function>
OneTimeMessageHandler::OneTimeMessageCallbackManager::CreateRespondingFunction(
    ScriptContext& script_context,
    const PortId& port_id,
    std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback> callback) {
  return CreateDelayedOneTimeMessageCallback(
      script_context, port_id, std::move(callback),
      /*cleanup_if_function_unused=*/true);
}

v8::Local<v8::Function> OneTimeMessageHandler::OneTimeMessageCallbackManager::
    CreateEventDispatchFunction(
        ScriptContext& script_context,
        const PortId& port_id,
        std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback>
            callback) {
  return CreateDelayedOneTimeMessageCallback(
      script_context, port_id, std::move(callback),
      /*cleanup_if_function_unused=*/false);
}

v8::Local<v8::Function> OneTimeMessageHandler::OneTimeMessageCallbackManager::
    CreateListenerThrowsErrorFunction(
        ScriptContext& script_context,
        const PortId& port_id,
        std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback> callback,
        const CallbackID& callback_id) {
  return CreateDelayedOneTimeMessageCallback(
      script_context, port_id, std::move(callback),
      /*cleanup_if_function_unused=*/false, callback_id);
}

void OneTimeMessageHandler::OneTimeMessageCallbackManager::
    ClearCallbackDataForPortId(ScriptContext* script_context,
                               const PortId& port_id) {
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(script_context->v8_context(),
                                                   kDontCreateIfMissing);
  if (!data) {
    return;
  }

  data->pending_receiver_callbacks.erase(port_id);
}

void OneTimeMessageHandler::OneTimeMessageCallbackManager::
    DeleteCallbackDataForCallbackId(
        ScriptContext* script_context,
        const PortId& port_id,
        const OneTimeMessageHandler::CallbackID& callback_id) {
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(script_context->v8_context(),
                                                   kDontCreateIfMissing);
  if (!data) {
    return;
  }

  if (auto port_iter = data->pending_receiver_callbacks.find(port_id);
      port_iter != data->pending_receiver_callbacks.end()) {
    port_iter->second.erase(callback_id);
  }
}

int OneTimeMessageHandler::OneTimeMessageCallbackManager::
    GetPendingCallbackCountForTest(ScriptContext* script_context,
                                   const PortId& port_id) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(script_context->v8_context(),
                                                   kDontCreateIfMissing);

  if (!data) {
    return 0;
  }

  if (auto port_iter = data->pending_receiver_callbacks.find(port_id);
      port_iter != data->pending_receiver_callbacks.end()) {
    return port_iter->second.size();
  }

  return 0;
}

OneTimeMessageHandler::OneTimeMessageHandler(
    NativeExtensionBindingsSystem* bindings_system)
    : bindings_system_(bindings_system),
      callback_manager_(
          std::make_unique<
              OneTimeMessageHandler::OneTimeMessageCallbackManager>(*this)) {}
OneTimeMessageHandler::~OneTimeMessageHandler() = default;

bool OneTimeMessageHandler::HasPort(ScriptContext* script_context,
                                    const PortId& port_id) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(script_context->v8_context(),
                                                   kDontCreateIfMissing);
  if (!data)
    return false;
  return port_id.is_opener ? base::Contains(data->openers, port_id)
                           : base::Contains(data->receivers, port_id);
}

v8::Local<v8::Promise> OneTimeMessageHandler::SendMessage(
    ScriptContext* script_context,
    const PortId& new_port_id,
    const MessageTarget& target,
    mojom::ChannelType channel_type,
    const Message& message,
    binding::AsyncResponseType async_type,
    v8::Local<v8::Function> response_callback,
    mojom::MessagePortHost* message_port_host,
    mojo::PendingAssociatedRemote<mojom::MessagePort> message_port,
    mojo::PendingAssociatedReceiver<mojom::MessagePortHost>
        message_port_host_receiver) {
  v8::Isolate* isolate = script_context->isolate();
  v8::EscapableHandleScope handle_scope(isolate);

  DCHECK(new_port_id.is_opener);
  DCHECK_EQ(script_context->context_id(), new_port_id.context_id);

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(script_context->v8_context(),
                                                   kCreateIfMissing);
  DCHECK(data);

  v8::Local<v8::Promise> promise;
  bool wants_response = async_type != binding::AsyncResponseType::kNone;
  if (wants_response) {
    // If this is a promise based request no callback should have been passed
    // in.
    if (async_type == binding::AsyncResponseType::kPromise)
      DCHECK(response_callback.IsEmpty());

    APIRequestHandler::RequestDetails details =
        bindings_system_->api_system()->request_handler()->AddPendingRequest(
            script_context->v8_context(), async_type, response_callback,
            binding::ResultModifierFunction());
    OneTimeOpener& port = data->openers[new_port_id];
    port.request_id = details.request_id;
    port.async_type = async_type;
    port.channel_type = channel_type;
    promise = details.promise;
    DCHECK_EQ(async_type == binding::AsyncResponseType::kPromise,
              !promise.IsEmpty());
  }

  IPCMessageSender* ipc_sender = bindings_system_->GetIPCMessageSender();
  std::string channel_name;
  switch (channel_type) {
    case mojom::ChannelType::kSendRequest:
      channel_name = messaging_util::kSendRequestChannel;
      break;
    case mojom::ChannelType::kSendMessage:
      channel_name = messaging_util::kSendMessageChannel;
      break;
    case mojom::ChannelType::kNative:
      // Native messaging doesn't use channel names.
      break;
    case mojom::ChannelType::kConnect:
      // connect() calls aren't handled by the OneTimeMessageHandler.
      NOTREACHED();
  }

  ipc_sender->SendOpenMessageChannel(
      script_context, new_port_id, target, channel_type, channel_name,
      std::move(message_port), std::move(message_port_host_receiver));
  message_port_host->PostMessage(message);

  // If the sender doesn't provide a response callback, we can immediately
  // close the channel. Note: we only do this for extension messages, not
  // native apps.
  // TODO(devlin): This is because of some subtle ordering in the browser side,
  // where closing the channel after sending the message causes things to be
  // destroyed in the wrong order. That would be nice to fix.
  if (!wants_response && target.type != MessageTarget::NATIVE_APP) {
    message_port_host->ClosePort(/*close_channel=*/true,
                                 /*error_message=*/std::nullopt);
  }

  return handle_scope.Escape(promise);
}

void OneTimeMessageHandler::AddReceiver(ScriptContext* script_context,
                                        const PortId& target_port_id,
                                        v8::Local<v8::Object> sender,
                                        const std::string& event_name) {
  DCHECK(!target_port_id.is_opener);
  DCHECK_NE(script_context->context_id(), target_port_id.context_id);

  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = script_context->v8_context();

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(context, kCreateIfMissing);
  DCHECK(data);
  DCHECK(!base::Contains(data->receivers, target_port_id));
  OneTimeReceiver& receiver = data->receivers[target_port_id];
  receiver.sender.Reset(isolate, sender);
  receiver.event_name = event_name;
}

void OneTimeMessageHandler::AddReceiverForTesting(
    ScriptContext* script_context,
    const PortId& target_port_id,
    v8::Local<v8::Object> sender,
    const std::string& event_name,
    mojo::PendingAssociatedRemote<mojom::MessagePort>& message_port_remote,
    mojo::PendingAssociatedReceiver<mojom::MessagePortHost>&
        message_port_host_receiver) {
  AddReceiver(script_context, target_port_id, sender, event_name);
  messaging_service()->BindPortForTesting(  // IN-TEST
      script_context, target_port_id, message_port_remote,
      message_port_host_receiver);
}

bool OneTimeMessageHandler::DeliverMessage(ScriptContext* script_context,
                                           const Message& message,
                                           const PortId& target_port_id) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  return target_port_id.is_opener
             ? DeliverReplyToOpener(script_context, message, target_port_id)
             : DeliverMessageToReceiver(script_context, message,
                                        target_port_id);
}

bool OneTimeMessageHandler::Disconnect(ScriptContext* script_context,
                                       const PortId& port_id,
                                       const std::string& error_message) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  return port_id.is_opener
             ? DisconnectOpener(script_context, port_id, error_message)
             : DisconnectReceiver(script_context, port_id);
}

int OneTimeMessageHandler::GetPendingCallbackCountForTest(
    ScriptContext* script_context,
    const PortId& port_id) {
  return callback_manager_->GetPendingCallbackCountForTest(  // IN-TEST
      script_context, port_id);
}

std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback>
OneTimeMessageHandler::CreateMessageResponseCallback(const PortId& port_id) {
  return std::make_unique<OneTimeMessageHandler::OneTimeMessageCallback>(
      base::BindOnce(&OneTimeMessageHandler::OnOneTimeMessageResponse,
                     weak_factory_.GetWeakPtr(), port_id));
}

std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback>
OneTimeMessageHandler::CreatePromiseRejectedCallback(const PortId& port_id) {
  return std::make_unique<OneTimeMessageHandler::OneTimeMessageCallback>(
      base::BindOnce(&OneTimeMessageHandler::OnPromiseRejectedResponse,
                     weak_factory_.GetWeakPtr(), port_id));
}

std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback>
OneTimeMessageHandler::CreateEventDispatchCallback(
    const PortId& port_id,
    std::optional<CallbackID> listener_error_callback_id) {
  return std::make_unique<OneTimeMessageHandler::OneTimeMessageCallback>(
      base::BindOnce(&OneTimeMessageHandler::OnEventFired,
                     weak_factory_.GetWeakPtr(), port_id,
                     listener_error_callback_id));
}

std::unique_ptr<OneTimeMessageHandler::OneTimeMessageCallback>
OneTimeMessageHandler::CreateListenerErrorCallback(const PortId& port_id) {
  return std::make_unique<OneTimeMessageHandler::OneTimeMessageCallback>(
      base::BindOnce(&OneTimeMessageHandler::OnListenerThrowsError,
                     weak_factory_.GetWeakPtr(), port_id));
}

void OneTimeMessageHandler::OnAllCallbacksCollected(
    ScriptContext* script_context,
    v8::Local<v8::Context> context,
    const PortId& port_id) {
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(script_context->v8_context(),
                                                   kDontCreateIfMissing);
  if (!data) {
    return;
  }
  auto iter = data->receivers.find(port_id);
  // The channel may already be closed (if the receiver replied before the reply
  // callback was collected).
  if (iter == data->receivers.end()) {
    return;
  }
  // Since no more callbacks can be called the receiver doesn't need to be
  // tracked anymore.
  data->receivers.erase(port_id);

  // A different receiver may reply so don't close the channel.
  CloseReceiverMessagePortOrChannel(script_context, port_id,
                                    /*close_channel=*/false,
                                    /*error=*/std::nullopt);
}

bool OneTimeMessageHandler::DeliverMessageToReceiver(
    ScriptContext* script_context,
    const Message& message,
    const PortId& target_port_id) {
  DCHECK(!target_port_id.is_opener);

  v8::Isolate* isolate = script_context->isolate();
  v8::Local<v8::Context> context = script_context->v8_context();

  bool handled = false;

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(context,
                                                   kDontCreateIfMissing);
  if (!data)
    return handled;

  auto iter = data->receivers.find(target_port_id);
  if (iter == data->receivers.end())
    return handled;

  handled = true;
  OneTimeReceiver& port = iter->second;

  // This port is a receiver, so we invoke the onMessage event and provide a
  // callback through which the port can respond. The port stays open until we
  // receive a response.
  auto message_response_callback =
      CreateMessageResponseCallback(target_port_id);
  // The v8 `reply` (a.k.a `sendResponse()`) function provided to
  // `runtime.onMessage` listeners.
  v8::Local<v8::Function> message_response_function =
      callback_manager_->CreateRespondingFunction(
          *script_context, target_port_id,
          std::move(message_response_callback));

  if (IsMessagePolyfillSupportEnabled()) {
    port.message_response_function =
        v8::Global<v8::Function>(isolate, message_response_function);
  }

  v8::HandleScope handle_scope(isolate);

  // The current port is a receiver. The parsing should be fail-safe if this is
  // a receiver for a native messaging host (i.e. the event name is
  // kOnConnectNativeEvent). This is because a native messaging host can send
  // malformed messages.
  std::string error;
  v8::Local<v8::Value> v8_message = messaging_util::MessageToV8(
      context, message,
      port.event_name == messaging_util::kOnConnectNativeEvent, &error);

  if (error.empty()) {
    v8::Local<v8::Object> v8_sender = port.sender.Get(isolate);
    v8::LocalVector<v8::Value> args(
        isolate, {v8_message, v8_sender, message_response_function});

    v8::Local<v8::Function> message_dispatched_function;
    v8::Local<v8::Function> listener_throws_error_function;
    // For runtime.onMessage, we require that the listener indicate if they
    // intend to respond asynchronously. `message_dispatched_callback` will
    // check the results of the listeners to determine if a listener indicated
    // it intended to respond asynchronously.
    if (port.event_name == messaging_util::kOnMessageEvent) {
      CallbackID listener_throws_error_callback_id;
      if (IsMessagePolyfillSupportEnabled()) {
        auto listener_throws_error_callback =
            CreateListenerErrorCallback(target_port_id);
        listener_throws_error_callback_id = CallbackID::Create();
        listener_throws_error_function =
            callback_manager_->CreateListenerThrowsErrorFunction(
                *script_context, target_port_id,
                std::move(listener_throws_error_callback),
                listener_throws_error_callback_id);
      }
      auto message_dispatched_callback = CreateEventDispatchCallback(
          target_port_id, listener_throws_error_callback_id);
      message_dispatched_function =
          callback_manager_->CreateEventDispatchFunction(
              *script_context, target_port_id,
              std::move(message_dispatched_callback));
    }
    bindings_system_->api_system()->event_handler()->FireEventInContext(
        port.event_name, context, &args, /*filter=*/nullptr,
        message_dispatched_function, listener_throws_error_function);
  } else {
    console::AddMessage(script_context,
                        blink::mojom::ConsoleMessageLevel::kError, error);
  }

  // Note: The context could be invalidated at this point!

  return handled;
}

bool OneTimeMessageHandler::DeliverReplyToOpener(ScriptContext* script_context,
                                                 const Message& message,
                                                 const PortId& target_port_id) {
  DCHECK(target_port_id.is_opener);

  v8::Local<v8::Context> v8_context = script_context->v8_context();
  bool handled = false;

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(v8_context,
                                                   kDontCreateIfMissing);
  if (!data)
    return handled;

  auto iter = data->openers.find(target_port_id);
  if (iter == data->openers.end())
    return handled;

  handled = true;

  // Note: make a copy of port, since we're about to free it.
  const OneTimeOpener port = iter->second;
  DCHECK_NE(-1, port.request_id);

  // We erase the opener now, since delivering the reply can cause JS to run,
  // which could either invalidate the context or modify the |openers|
  // collection (e.g., by sending another message).
  data->openers.erase(iter);

  // This port was the opener, so the message is the response from the
  // receiver. Invoke the callback and close the message port.
  v8::Isolate* isolate = script_context->isolate();

  // Parsing should be fail-safe for kNative channel type as native messaging
  // hosts can send malformed messages.
  std::string error;
  v8::Local<v8::Value> v8_message = messaging_util::MessageToV8(
      v8_context, message, port.channel_type == mojom::ChannelType::kNative,
      &error);

  if (v8_message.IsEmpty()) {
    // If the parsing fails, send back a v8::Undefined() message.
    v8_message = v8::Undefined(isolate);
  }

  v8::LocalVector<v8::Value> args(isolate, {v8_message});
  bindings_system_->api_system()->request_handler()->CompleteRequest(
      port.request_id, args, error);

  bindings_system_->messaging_service()->CloseMessagePort(
      script_context, target_port_id, /*close_channel=*/true);

  // Note: The context could be invalidated at this point!

  return handled;
}

bool OneTimeMessageHandler::DisconnectReceiver(ScriptContext* script_context,
                                               const PortId& port_id) {
  v8::Local<v8::Context> context = script_context->v8_context();
  bool handled = false;

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(context,
                                                   kDontCreateIfMissing);
  if (!data)
    return handled;

  auto iter = data->receivers.find(port_id);
  if (iter == data->receivers.end())
    return handled;

  handled = true;
  // With the channel closed, clean up the receiver port and its pending
  // callbacks. This prevents further responses and avoids callback data leaks
  // from indicated-but-never-sent asynchronous replies from the listener(s).
  data->receivers.erase(iter);
  callback_manager_->ClearCallbackDataForPortId(script_context, port_id);

  // The `ExtensionMessagePort` for this receiver's destructor handles message
  // port (IPC) cleanup so we don't need to do that here.
  return handled;
}

bool OneTimeMessageHandler::DisconnectOpener(ScriptContext* script_context,
                                             const PortId& port_id,
                                             const std::string& error_message) {
  bool handled = false;

  v8::Local<v8::Context> v8_context = script_context->v8_context();
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(v8_context,
                                                   kDontCreateIfMissing);
  if (!data)
    return handled;

  auto iter = data->openers.find(port_id);
  if (iter == data->openers.end())
    return handled;

  handled = true;

  // Note: make a copy of port, since we're about to free it.
  const OneTimeOpener opener = iter->second;
  DCHECK_NE(-1, opener.request_id);

  // We erase the opener now, since delivering the reply can cause JS to run,
  // which could either invalidate the context or modify the |openers|
  // collection (e.g., by sending another message).
  data->openers.erase(iter);

  std::string error;
  // Set the error for the message port. If the browser supplies an error, we
  // always use that. Otherwise, the behavior is different for promise-based vs
  // callback-based channels.
  // For a promise-based channel, not receiving a response is fine (assuming the
  // listener didn't indicate it would send one) - the extension may simply be
  // waiting for confirmation that the message sent.
  // In the callback-based scenario, we use the presence of the callback as an
  // indication that the extension expected a specific response. This is an
  // unfortunate behavior difference that we keep for backwards-compatibility in
  // callback-based API calls.
  if (!error_message.empty()) {
    // If the browser supplied us with an error message, use that.
    error = error_message;
  } else if (opener.async_type == binding::AsyncResponseType::kCallback) {
    error = "The message port closed before a response was received.";
  }

  bindings_system_->api_system()->request_handler()->CompleteRequest(
      opener.request_id, v8::LocalVector<v8::Value>(v8::Isolate::GetCurrent()),
      error);

  // Note: The context could be invalidated at this point!

  return handled;
}

void OneTimeMessageHandler::CloseReceiverMessagePortOrChannel(
    ScriptContext* script_context,
    const PortId& port_id,
    bool close_channel,
    std::optional<std::string> error) {
  // With the message port closing callbacks aren't allowed to be called after
  // this point so proactively clean them up.
  callback_manager_->ClearCallbackDataForPortId(script_context, port_id);

  // If there was an error send it back to the message sender.
  if (close_channel && error) {
    messaging_service()->CloseMessagePort(script_context, port_id,
                                          close_channel, *error);
    return;
  }

  // Otherwise if no error then just close the port and/or channel.
  messaging_service()->CloseMessagePort(script_context, port_id, close_channel);
}

void OneTimeMessageHandler::OnOneTimeMessageResponse(
    const PortId& port_id,
    gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // The listener may try replying after the context or the channel has been
  // closed. Fail gracefully.
  // TODO(devlin): At least in the case of the channel being closed (e.g.
  // because the listener did not indicate it would reply asynchronously), it
  // might be good to surface an error.
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(context,
                                                   kDontCreateIfMissing);
  if (!data)
    return;

  auto iter = data->receivers.find(port_id);
  // The channel may already be closed (if a listener replied (promise rejected)
  // or listener threw error).
  if (iter == data->receivers.end())
    return;
  // The response will be sent after this point so we no longer need to track
  // the receiver.
  data->receivers.erase(port_id);

  v8::Local<v8::Value> value;
  // We allow omitting the message argument (e.g., sendMessage()). Default the
  // value to undefined.
  if (arguments->Length() > 0)
    CHECK(arguments->GetNext(&value));
  else
    value = v8::Undefined(isolate);

  ScriptContext* script_context = GetScriptContextFromV8Context(context);

  std::string message_creation_error;
  std::unique_ptr<Message> message = messaging_util::MessageFromV8(
      context, value, port_id.serialization_format, &message_creation_error);
  if (!message) {
    // Throw an error in the listener context.
    arguments->ThrowTypeError(message_creation_error);
    if (IsMessagePolyfillSupportEnabled()) {
      // This is a "fatal" error for the channel so close it entirely.
      CloseReceiverMessagePortOrChannel(script_context, port_id,
                                        /*close_channel=*/true,
                                        message_creation_error);
    }
    return;
  }

  // If the MessagePortHost is still alive return the response. But the listener
  // might be replying after the channel has been closed.
  if (auto* message_port_host = messaging_service()->GetMessagePortHostIfExists(
          script_context, port_id)) {
    message_port_host->PostMessage(*message);
    CloseReceiverMessagePortOrChannel(script_context, port_id,
                                      /*close_channel=*/true,
                                      /*error=*/std::nullopt);
  }

  // With the message port closed no more callbacks should be called.
  callback_manager_->ClearCallbackDataForPortId(script_context, port_id);
}

v8::Local<v8::Function> OneTimeMessageHandler::OneTimeMessageCallbackManager::
    CreateDelayedOneTimeMessageCallback(
        ScriptContext& script_context,
        const PortId& port_id,
        std::unique_ptr<OneTimeMessageCallback> callback,
        bool cleanup_if_function_unused) {
  return CreateDelayedOneTimeMessageCallback(
      script_context, port_id, std::move(callback), cleanup_if_function_unused,
      /*optional_callback_id=*/std::nullopt);
}

v8::Local<v8::Function> OneTimeMessageHandler::OneTimeMessageCallbackManager::
    CreateDelayedOneTimeMessageCallback(
        ScriptContext& script_context,
        const PortId& port_id,
        std::unique_ptr<OneTimeMessageCallback> callback,
        bool cleanup_if_function_unused,
        std::optional<CallbackID> optional_callback_id) {
  CHECK(callback);
  v8::Isolate* isolate = script_context.isolate();
  v8::Local<v8::Context> context = script_context.v8_context();

  // We shouldn't need to check and get `data` like this if a listener has
  // already responded, but it's much simpler to re-get it here than pass
  // OneTimeMessageContextData into this method.
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(
          context, CreatePerContextData::kDontCreateIfMissing);
  // We will store `callback` in the per context data for later retrieval so it
  // must exist for us to proceed.
  CHECK(data);

  CallbackID callback_id;
  if (optional_callback_id) {
    callback_id = *optional_callback_id;
  } else {
    callback_id = OneTimeMessageHandler::CallbackID::Create();
  }

  // We convert to a v8::String here because we want to validate the string is
  // still a valid `CallbackID` when we retrieve it from v8 when `function` is
  // called.
  v8::Local<v8::String> callback_id_v8_string =
      gin::StringToV8(isolate, callback_id.ToString());
  v8::Local<v8::Function> function;
  if (!v8::Function::New(context, &DelayedOneTimeMessageCallbackHelper,
                         callback_id_v8_string)
           .ToLocal(&function)) {
    NOTREACHED();
  }

  auto& port_callbacks = data->pending_receiver_callbacks[port_id];
  const auto& [callback_id_iter, callback_id_inserted] =
      port_callbacks.try_emplace(callback_id, std::move(callback));
  // It could lead to unexpected behavior to add the same callback multiple
  // times for the same one time message port.
  CHECK(callback_id_inserted);

  if (cleanup_if_function_unused) {
    new GCCallback(
        &script_context, function,
        /*callback=*/
        base::BindOnce(&OneTimeMessageHandler::OneTimeMessageCallbackManager::
                           OnDelayedOneTimeMessageCallbackCollected,
                       weak_factory_.GetWeakPtr(), &script_context, port_id,
                       callback_id),
        /*fallback=*/base::OnceClosure());
  }

  return function;
}

void OneTimeMessageHandler::OneTimeMessageCallbackManager::
    OnDelayedOneTimeMessageCallbackCollected(ScriptContext* script_context,
                                             const PortId& port_id,
                                             CallbackID callback_id) {
  // Note: we know |script_context| is still valid because the GC callback won't
  // be called after context invalidation.
  v8::HandleScope handle_scope(script_context->isolate());
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(script_context->v8_context(),
                                                   kDontCreateIfMissing);
  // ScriptContext invalidation and PerContextData cleanup happen "around" the
  // same time, but there aren't strict guarantees about ordering. It's possible
  // the data was collected.
  if (!data)
    return;

  // Since there is no way to call the callback anymore, we can remove it from
  // the pending callbacks and delete the port entry if this was the last
  // callback. Note: this should occur before returning early due to the
  // receiver being deleted because multiple pending callbacks can be created
  // for each message or `DisconnectReceiver()` could be called before we get
  // here.
  if (auto port_id_iter = data->pending_receiver_callbacks.find(port_id);
      port_id_iter != data->pending_receiver_callbacks.end()) {
    auto& callbacks = port_id_iter->second;
    callbacks.erase(callback_id);
    if (!callbacks.empty()) {
      // If we've deleted the callback, but there's still a remaining callback
      // then this should only happen iff polyfill support is enabled.
      DCHECK(IsMessagePolyfillSupportEnabled());
      // When polyfill support is enabled we'll create two callbacks (message
      // response and promise reject) that can be collected at different times.
      // Only the last callback of these two collected should continue on to
      // close the port. Otherwise it could cause the other callback to not
      // fully run if called because it'll think the port was already closed.
      return;
    }
    // There are no more callbacks remaining, so delete the unused `PortId` key.
    data->pending_receiver_callbacks.erase(port_id_iter);
  }

  // Notify `message_handler_` so it can update the port state.
  message_handler_->OnAllCallbacksCollected(
      script_context, script_context->v8_context(), port_id);
  // More callbacks could be collected later so we'll leave the callback data
  // alone after closing the port.
}

std::optional<std::string> OneTimeMessageHandler::GetErrorMessageFromValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> possible_error_value) {
  if (!possible_error_value->IsNativeError()) {
    return std::nullopt;
  }

  v8::Local<v8::Message> error_message =
      v8::Exception::CreateMessage(isolate, possible_error_value);
  std::string error_message_from_v8;
  bool error_message_string_convert_success =
      gin::Converter<std::string>::FromV8(isolate,
                                          error_message->Get().As<v8::Value>(),
                                          &error_message_from_v8);

  if (!error_message_string_convert_success || error_message_from_v8.empty()) {
    return std::nullopt;
  }

  return error_message_from_v8;
}

void OneTimeMessageHandler::OnPromiseRejectedResponse(
    const PortId& port_id,
    gin::Arguments* arguments) {
  CHECK(IsMessagePolyfillSupportEnabled());

  CHECK(arguments);
  v8::Isolate* isolate = arguments->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // The promise may reject after the context or the channel has been closed.
  // Fail gracefully.
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(context,
                                                   kDontCreateIfMissing);
  if (!data) {
    return;
  }
  auto iter = data->receivers.find(port_id);
  // The channel may already be closed (if a listener already replied, or
  // listener threw error).
  if (iter == data->receivers.end()) {
    return;
  }
  // The promise reject will be sent as an error response after this point so we
  // no longer need to track the receiver.
  data->receivers.erase(port_id);

  v8::Local<v8::Value> promise_reject_value;
  // This is safe to CHECK() because when a promise rejects it always provides a
  // value. Even if `reject()` (with no argument) is called we see `undefined`
  // for `promise_reject_value`.
  CHECK(arguments->Length() > 0);
  CHECK(arguments->GetNext(&promise_reject_value));

  // If promise rejection reason is a JS Error type then close the message port
  // with the Error's .message property. Otherwise return a generic error
  // message.
  // TODO(crbug.com/439644930): Support sending the listener's stack trace along
  // with the rejection error. mozilla/webextension-polyfill doesn't support it
  // currently, but plans to (see
  // https://github.com/mozilla/webextension-polyfill/issues/210).
  std::optional<std::string> error_message_from_value =
      GetErrorMessageFromValue(isolate, promise_reject_value);

  std::string error_message =
      error_message_from_value
          ? *error_message_from_value
          : "A runtime.onMessage listener's promise rejected without an Error";

  // TODO(crbug.com/439644930): Support sending the listener's stack trace along
  // with the rejection error. mozilla/webextension-polyfill doesn't support it
  // currently, but plans to (see
  // https://github.com/mozilla/webextension-polyfill/issues/210).

  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  CloseReceiverMessagePortOrChannel(script_context, port_id,
                                    /*close_channel=*/true, error_message);
}

void OneTimeMessageHandler::OnListenerThrowsError(const PortId& port_id,
                                                  gin::Arguments* arguments) {
  CHECK(IsMessagePolyfillSupportEnabled());

  v8::Isolate* isolate = arguments->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(context,
                                                   kDontCreateIfMissing);

  // Dispatching can invalidate the context so if it is then we won't be able to
  // inform the message sender.
  if (!data) {
    return;
  }

  auto iter = data->receivers.find(port_id);
  // The channel may already be closed (if a listener already replied).
  if (iter == data->receivers.end()) {
    return;
  }
  // The listener thrown error will be sent as an error response after this
  // point so we no longer need to track the receiver.
  data->receivers.erase(port_id);

  v8::Local<v8::Value> listener_thrown_value;
  CHECK(arguments->Length() > 0);
  CHECK(arguments->GetNext(&listener_thrown_value));

  std::optional<std::string> error_message_from_value =
      GetErrorMessageFromValue(isolate, listener_thrown_value);
  std::string error_message =
      error_message_from_value
          ? *error_message_from_value
          : "Error message from listener couldn't be parsed or was empty.";

  // TODO(crbug.com/439644930): Support sending the listener's stack trace along
  // with the rejection error. mozilla/webextension-polyfill doesn't support it
  // currently, but plans to (see
  // https://github.com/mozilla/webextension-polyfill/issues/210).

  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  CloseReceiverMessagePortOrChannel(script_context, port_id,
                                    /*close_channel=*/true, error_message);
}

bool OneTimeMessageHandler::CheckAndHandleAsyncListenerReply(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    ScriptContext& script_context,
    v8::Local<v8::Value> result,
    const PortId& port_id,
    // TODO(crbug.com/40753031): Move the creation of
    // `promise_resolved_function` to just before promise handler attachment. It
    // doesn't need to be created before that point.
    v8::Local<v8::Function> promise_resolved_function) {
  v8::Local<v8::Array> results_array =
      GetListenerResultArray(isolate, context, result, "results");
  if (results_array.IsEmpty()) {
    return false;
  }

  bool will_reply_async = false;
  for (uint32_t i = 0; i < results_array->Length(); ++i) {
    v8::MaybeLocal<v8::Value> maybe_result = results_array->Get(context, i);
    v8::Local<v8::Value> listener_return;
    // Assume the result could throw due to changes at runtime by the
    // extension's JS code.
    if (!maybe_result.ToLocal(&listener_return)) {
      continue;
    }

    // Check if any of the results is indicating it will reply async by
    // returning `true`.
    if (listener_return->IsBoolean() &&
        listener_return.As<v8::Boolean>()->Value()) {
      will_reply_async = true;
    }

    // If promise returns are not supported, then we don't need to attach any
    // callbacks and can return early once we find at least one listener that
    // wants to reply asynchronously
    if (!IsMessagePolyfillSupportEnabled() && will_reply_async) {
      return true;
    }

    // Check if any of the returns are a promise, indicating the listener will
    // reply async. Attach callbacks for both the promise resolving or
    // rejecting. This is so that whatever the promise settles to is considered
    // the listener replying to the message sender with the settled value.
    if (IsMessagePolyfillSupportEnabled() && listener_return->IsPromise()) {
      auto promise_rejected_response_callback =
          CreatePromiseRejectedCallback(port_id);
      v8::Local<v8::Function> promise_rejected_function =
          callback_manager_->CreateRespondingFunction(
              script_context, port_id,
              std::move(promise_rejected_response_callback));
      std::ignore = listener_return.As<v8::Promise>()->Then(
          context, promise_resolved_function, promise_rejected_function);
      // TODO(crbug.com/40753031): Consider setting lastError for caller when
      // promise is rejected.
      will_reply_async = true;
    }
  }

  return will_reply_async;
}

void OneTimeMessageHandler::OnEventFired(
    const PortId& port_id,
    std::optional<CallbackID> listener_error_callback_id,
    gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Value> result;
  if (arguments->Length() > 0) {
    CHECK(arguments->GetNext(&result));
  } else {
    result = v8::Undefined(isolate);
  }

  // The context could be tearing down by the time the event is fully
  // dispatched.
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(context,
                                                   kDontCreateIfMissing);
  if (!data)
    return;

  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  DCHECK(script_context)
      << "script context was destroyed before runtime.onMessage listener "
         "results could be processed.";

  // Cleanup listener error callback if created since it shouldn't be possible
  // for synchronous thrown errors to appear after all listeners have finished
  // being dispatched to.
  if (IsMessagePolyfillSupportEnabled() && listener_error_callback_id) {
    callback_manager_->DeleteCallbackDataForCallbackId(
        script_context, port_id, *listener_error_callback_id);
  }

  auto iter = data->receivers.find(port_id);
  // The channel may be closed (if the listener replied or threw an error).
  if (iter == data->receivers.end()) {
    return;
  }

  OneTimeReceiver& port = iter->second;

  v8::Local<v8::Function> promise_resolved_function;
  if (IsMessagePolyfillSupportEnabled()) {
    promise_resolved_function = port.message_response_function.Get(isolate);
    // Ensure the global function doesn't outlive port closing.
    port.message_response_function.SetWeak();
  }

  if (CheckAndHandleAsyncListenerReply(isolate, context, *script_context,
                                       result, port_id,
                                       promise_resolved_function)) {
    // Inform the browser that one of the listeners said they would be replying
    // later and leave the channel open.
    if (auto* message_port_host =
            messaging_service()->GetMessagePortHostIfExists(script_context,
                                                            port_id)) {
      message_port_host->ResponsePending();
    }
    return;
  }

  // The listener did not reply and did not indicate it would reply later from
  // any of its listeners. Close the message port. Don't close the channel
  // because another listener (in a separate context) may reply.
  data->receivers.erase(port_id);
  CloseReceiverMessagePortOrChannel(script_context, port_id,
                                    /*close_channel=*/false,
                                    /*error=*/std::nullopt);
}

// This must be defined in the .cc due to `OneTimeMessageHandler`'s header being
// included in `NativeExtensionBindingsSystem` causing a circular dependency.
NativeRendererMessagingService* OneTimeMessageHandler::messaging_service() {
  return bindings_system_->messaging_service();
}

}  // namespace extensions
