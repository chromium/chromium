// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/one_time_message_handler.h"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
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
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"
#include "ipc/ipc_message.h"
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

using OneTimeMessageCallback =
    base::OnceCallback<void(gin::Arguments* arguments)>;
struct OneTimeMessageContextData : public base::SupportsUserData::Data {
  static constexpr char kPerContextDataKey[] =
      "extension_one_time_message_context_data";

  std::map<PortId, OneTimeOpener> openers;
  std::map<PortId, OneTimeReceiver> receivers;
  std::map<OneTimeMessageHandler::CallbackID,
           std::unique_ptr<OneTimeMessageCallback>>
      pending_callbacks;
};

constexpr char OneTimeMessageContextData::kPerContextDataKey[];

bool OnMessagePromisesSupported() {
  return base::FeatureList::IsEnabled(
      extensions_features::kRuntimeOnMessagePromiseReturnSupport);
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
    // `data->pending_callbacks`, it will be garbage collected later in
    // `OnDelayedOneTimeMessageCallbackCollected()`.
    return;
  }

  auto iter = data->pending_callbacks.find(*callback_id);
  if (iter == data->pending_callbacks.end()) {
    // An extension may attempt to respond to a message multiple times despite
    // us only allowing the first response to be sent back to the sender. If
    // that happens, just return early to enforce this.
    return;
  }

  std::unique_ptr<OneTimeMessageCallback> callback = std::move(iter->second);
  data->pending_callbacks.erase(iter);
  std::move(*callback).Run(&arguments);
}

// Checks the listener `result` for any errors thrown by listeners. If any are
// found, this populates `error_message_out` with the first one found and
// returns true. Otherwise, returns false.
bool MaybeGetFirstErrorMessageFromListenerResult(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> result,
    std::string* error_message_out) {
  v8::Local<v8::Array> errors_array =
      GetListenerResultArray(isolate, context, result, "errors");
  if (errors_array.IsEmpty()) {
    return false;
  }

  uint32_t errors_count = errors_array->Length();

  // Search array for errors.
  for (uint32_t i = 0; i < errors_count; ++i) {
    v8::MaybeLocal<v8::Value> maybe_error = errors_array->Get(context, i);
    v8::Local<v8::Value> error;
    // Assume the result could throw due to changes at runtime by the
    // extension's JS code.
    if (!maybe_error.ToLocal(&error) && !error->IsNativeError()) {
      continue;
    }
    v8::Local<v8::Message> error_message =
        v8::Exception::CreateMessage(isolate, error);
    std::string error_message_from_v8;
    bool error_message_string_convert_success =
        gin::Converter<std::string>::FromV8(
            isolate, error_message->Get().As<v8::Value>(),
            &error_message_from_v8);
    if (error_message_string_convert_success &&
        !error_message_from_v8.empty()) {
      *error_message_out = error_message_from_v8;
    } else {
      *error_message_out =
          "Error message from listener couldn't be parsed or was empty.";
    }
    // An error was found.
    return true;
  }

  // No errors were found.
  return false;
}

base::debug::CrashKeyString* GetPromiseRejectFeatureEnabledCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_promise_reject_feature_enabled", base::debug::CrashKeySize::Size256);
  return crash_key;
}

}  // namespace

namespace debug {

// Helper for adding a crash keys when we encounter unexpected state in promise
// support for rejections.
//
// It is only created when the callback for a promise rejection is called to
// process the rejection's reason/value.
//
// All keys are logged every time this class is instantiated.
class ScopedPromiseRejectedResponseCrashKeys {
 public:
  explicit ScopedPromiseRejectedResponseCrashKeys(
      bool promise_support_feature_enabled)
      : promise_reject_feature_enabled_crash_key_(
            GetPromiseRejectFeatureEnabledCrashKey(),
            promise_support_feature_enabled ? "true" : "false") {}
  ~ScopedPromiseRejectedResponseCrashKeys() = default;

 private:
  // Records if the promise support feature was enabled as "true" or "false".
  base::debug::ScopedCrashKeyString promise_reject_feature_enabled_crash_key_;
};

}  // namespace debug

OneTimeMessageHandler::OneTimeMessageHandler(
    NativeExtensionBindingsSystem* bindings_system)
    : bindings_system_(bindings_system) {}
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
  bindings_system_->messaging_service()->BindPortForTesting(  // IN-TEST
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
    ScriptContext* script_context) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(script_context->v8_context(),
                                                   kDontCreateIfMissing);
  return data ? data->pending_callbacks.size() : 0;
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
  // TODO(devlin): With chrome.runtime.sendMessage, we actually require that a
  // listener indicate if they intend to respond asynchronously; otherwise we
  // close the port.

  auto message_response_callback = std::make_unique<OneTimeMessageCallback>(
      base::BindOnce(&OneTimeMessageHandler::OnOneTimeMessageResponse,
                     weak_factory_.GetWeakPtr(), target_port_id));
  v8::Local<v8::Function> message_response_function =
      CreateDelayedOneTimeMessageCallback(isolate, context, target_port_id,
                                          std::move(message_response_callback),
                                          script_context,
                                          /*close_port_on_collection=*/true);

  if (OnMessagePromisesSupported()) {
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
    // For runtime.onMessage, we require that the listener indicate if they
    // intend to respond asynchronously. Check the results of the listeners.
    if (port.event_name == messaging_util::kOnMessageEvent) {
      auto message_dispatched_callback =
          std::make_unique<OneTimeMessageCallback>(
              base::BindOnce(&OneTimeMessageHandler::OnEventFired,
                             weak_factory_.GetWeakPtr(), target_port_id));
      message_dispatched_function = CreateDelayedOneTimeMessageCallback(
          isolate, context, target_port_id,
          std::move(message_dispatched_callback), script_context,
          /*close_port_on_collection=*/false);
    }
    bindings_system_->api_system()->event_handler()->FireEventInContext(
        port.event_name, context, &args, nullptr, message_dispatched_function);
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
  data->receivers.erase(iter);
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
  if (iter == data->receivers.end())
    return;

  data->receivers.erase(iter);

  v8::Local<v8::Value> value;
  // We allow omitting the message argument (e.g., sendMessage()). Default the
  // value to undefined.
  if (arguments->Length() > 0)
    CHECK(arguments->GetNext(&value));
  else
    value = v8::Undefined(isolate);

  ScriptContext* script_context = GetScriptContextFromV8Context(context);

  std::string error;
  std::unique_ptr<Message> message = messaging_util::MessageFromV8(
      context, value, port_id.serialization_format, &error);
  if (!message) {
    // Throw an error in the listener context.
    arguments->ThrowTypeError(error);
    if (base::FeatureList::IsEnabled(
            extensions_features::
                kRuntimeOnMessageWebExtensionPolyfillSupport)) {
      NativeRendererMessagingService* messaging_service =
          bindings_system_->messaging_service();
      messaging_service->CloseMessagePort(script_context, port_id,
                                          /*close_channel=*/true, error);
    }
    return;
  }

  // If the MessagePortHost is still alive return the response. But the listener
  // might be replying after the channel has been closed.
  if (auto* message_port_host =
          bindings_system_->messaging_service()->GetMessagePortHostIfExists(
              script_context, port_id)) {
    message_port_host->PostMessage(*message);
    bindings_system_->messaging_service()->CloseMessagePort(
        script_context, port_id, /*close_channel=*/true);
  }
}

// TODO(crbug.com/40753031): Pull out the functionality of wrapping delayed C++
// callbacks in v8::Functions out into a helper class so it's clearer to
// understand this mechanism now that it's used for more than one type of
// callback.
v8::Local<v8::Function>
OneTimeMessageHandler::CreateDelayedOneTimeMessageCallback(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const PortId& port_id,
    std::unique_ptr<OneTimeMessageCallback> callback,
    ScriptContext* script_context,
    bool close_port_on_collection) {
  CHECK(callback);

  // We shouldn't need to check and get `data` like this if a listener has
  // already responded, but it's much simpler to re-get it here than pass
  // OneTimeMessageContextData into this method.
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(
          context, CreatePerContextData::kDontCreateIfMissing);
  // We will store `callback` in the per context data for later retrieval so it
  // must exist for us to proceed.
  CHECK(data);

  CallbackID callback_id = CallbackID::Create();
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

  data->pending_callbacks[callback_id] = std::move(callback);

  if (close_port_on_collection) {
    new GCCallback(
        script_context, function,
        base::BindOnce(
            &OneTimeMessageHandler::OnDelayedOneTimeMessageCallbackCollected,
            weak_factory_.GetWeakPtr(), script_context, port_id, callback_id),
        base::OnceClosure());
  }

  return function;
}

void OneTimeMessageHandler::OnDelayedOneTimeMessageCallbackCollected(
    ScriptContext* script_context,
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
  // the pending callbacks. Note: this should occur before returning early
  // because multiple pending callbacks can be created for each message.
  data->pending_callbacks.erase(callback_id);

  // TODO(crbug.com/40753031): When the promise support feature is on this needs
  // to take into account if there are any other pending_callbacks that could be
  // run and not close the port if that is true. Otherwise, as-is, the
  // collection logic can close the port too early and prevent a response (or
  // error) from being sent back to the sender. For example if the message
  // response callback was never used and we get to this point, but the listener
  // returned a promise that hasn't settled yet, then the message response
  // callback here will delete the receiver which will then prevent the promise
  // callback from running since it will think the message port has already
  // closed.

  auto iter = data->receivers.find(port_id);
  // The channel may already be closed (if the receiver replied before the reply
  // callback was collected).
  if (iter == data->receivers.end())
    return;

  data->receivers.erase(iter);

  // Close the message port. There's no way to send a reply anymore. Don't
  // close the channel because another listener may reply.
  NativeRendererMessagingService* messaging_service =
      bindings_system_->messaging_service();
  messaging_service->CloseMessagePort(script_context, port_id,
                                      /*close_channel=*/false);
}

v8::Local<v8::Function> OneTimeMessageHandler::CreatePromiseRejectedFunction(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const PortId& port_id) {
  // Create the promise rejected callback.
  auto promise_rejected_response_callback =
      std::make_unique<OneTimeMessageCallback>(
          base::BindOnce(&OneTimeMessageHandler::PromiseRejectedResponse,
                         weak_factory_.GetWeakPtr(), port_id));
  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  // Store the callback so we can call it when/if the promise rejects.
  v8::Local<v8::Function> promise_rejected_function =
      CreateDelayedOneTimeMessageCallback(
          isolate, context, port_id,
          std::move(promise_rejected_response_callback), script_context,
          /*close_port_on_collection=*/true);

  return promise_rejected_function;
}

void OneTimeMessageHandler::PromiseRejectedResponse(const PortId& port_id,
                                                    gin::Arguments* arguments) {
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
  // The channel may already be closed (if a listener already replied).
  if (iter == data->receivers.end()) {
    return;
  }

  debug::ScopedPromiseRejectedResponseCrashKeys promise_rejected_crash_keys(
      /*promise_support_feature_enabled=*/OnMessagePromisesSupported());
  v8::Local<v8::Value> promise_reject_reason;
  // This is safe to CHECK() because when a promise rejects it always provides a
  // value. Even if `reject()` (with no argument) is called we see `undefined`
  // for `promise_reject_value`.
  CHECK(arguments->Length() > 0);
  CHECK(arguments->GetNext(&promise_reject_reason));

  // If promise rejection reason is a JS Error type then close the message port
  // with the Error's .message property. Otherwise return a generic error
  // message.
  // TODO(crbug.com/439644930): Support sending the listener's stack trace along
  // with the rejection error. mozilla/webextension-polyfill doesn't support it
  // currently, but plans to (see
  // https://github.com/mozilla/webextension-polyfill/issues/210).
  std::string promise_reject_error_message =
      "A runtime.onMessage listener's promise rejected without an Error";
  if (promise_reject_reason->IsNativeError()) {
    v8::Local<v8::Message> error_message =
        v8::Exception::CreateMessage(isolate, promise_reject_reason);
    std::string error_message_from_v8;
    bool error_message_string_convert_success =
        gin::Converter<std::string>::FromV8(
            isolate, error_message->Get().As<v8::Value>(),
            &error_message_from_v8);
    if (error_message_string_convert_success) {
      promise_reject_error_message = error_message_from_v8;
    }
  }

  // Prevent other listeners from responding since a listener returned promise
  // that settles is considered a (error) response.
  data->receivers.erase(iter);

  NativeRendererMessagingService* messaging_service =
      bindings_system_->messaging_service();
  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  messaging_service->CloseMessagePort(script_context, port_id,
                                      /*close_channel=*/true,
                                      promise_reject_error_message);
}

bool OneTimeMessageHandler::CheckAndHandleAsyncListenerReply(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
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

  uint32_t results_count = results_array->Length();

  for (uint32_t i = 0; i < results_count; ++i) {
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
      return true;
    }

    // Check if any of the returns are a promise -- indicating the listener
    // will reply async. If they do, handle both the promise resolving or
    // rejecting.
    if (OnMessagePromisesSupported() && listener_return->IsPromise()) {
      v8::Local<v8::Function> promise_rejected_function =
          CreatePromiseRejectedFunction(isolate, context, port_id);
      std::ignore = listener_return.As<v8::Promise>()->Then(
          context, promise_resolved_function, promise_rejected_function);
      // TODO(crbug.com/40753031): Consider setting lastError for caller when
      // promise is rejected.
      return true;
    }
  }

  return false;
}

void OneTimeMessageHandler::OnEventFired(const PortId& port_id,
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
  auto iter = data->receivers.find(port_id);
  // The channel may already be closed (if the listener replied).
  if (iter == data->receivers.end())
    return;

  OneTimeReceiver& port = iter->second;

  NativeRendererMessagingService* messaging_service =
      bindings_system_->messaging_service();
  ScriptContext* script_context = GetScriptContextFromV8Context(context);

  // If we find that a listener threw an error when attempting to respond to the
  // message, we consider that to be a message channel-closing event when
  // extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport is
  // enabled. Get the first listener error message seen and provide that back to
  // the message sender. This matches the behavior of
  // github.com/mozilla/webextension-polyfill.
  std::string first_listener_error_message;
  if (base::FeatureList::IsEnabled(
          extensions_features::kRuntimeOnMessageWebExtensionPolyfillSupport) &&
      MaybeGetFirstErrorMessageFromListenerResult(
          isolate, context, result, &first_listener_error_message)) {
    // TODO(crbug.com/439644930): Support sending the listener's stack trace
    // along with the rejection error. mozilla/webextension-polyfill doesn't
    // support it currently, but plans to (see
    // https://github.com/mozilla/webextension-polyfill/issues/210).
    messaging_service->CloseMessagePort(script_context, port_id,
                                        /*close_channel=*/true,
                                        first_listener_error_message);
    return;
  }

  v8::Local<v8::Function> promise_resolved_function;
  if (OnMessagePromisesSupported()) {
    promise_resolved_function = port.message_response_function.Get(isolate);
  }

  if (CheckAndHandleAsyncListenerReply(isolate, context, result, port_id,
                                       promise_resolved_function)) {
    if (OnMessagePromisesSupported()) {
      // Ensure the global function doesn't outlive port closing.
      port.message_response_function.SetWeak();
    }
    // Inform the browser that one of the listeners said they would be replying
    // later and leave the channel open.
    if (auto* message_port_host = messaging_service->GetMessagePortHostIfExists(
            script_context, port_id)) {
      message_port_host->ResponsePending();
    }
    return;
  }

  data->receivers.erase(iter);

  // The listener did not reply and did not indicate it would reply later from
  // any of its listeners. Close the message port. Don't close the channel
  // because another listener (in a separate context) may reply.
  messaging_service->CloseMessagePort(script_context, port_id,
                                      /*close_channel=*/false);
}

}  // namespace extensions
