// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/one_time_message_handler.h"

#include <map>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/supports_user_data.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
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
};

using OneTimeMessageCallback =
    base::OnceCallback<void(gin::Arguments* arguments)>;
struct OneTimeMessageContextData : public base::SupportsUserData::Data {
  static constexpr char kPerContextDataKey[] =
      "extension_one_time_message_context_data";

  std::map<PortId, OneTimeOpener> openers;
  std::map<PortId, OneTimeReceiver> receivers;
  std::vector<std::unique_ptr<OneTimeMessageCallback>> pending_callbacks;
};

constexpr char OneTimeMessageContextData::kPerContextDataKey[];

void OneTimeMessageResponseHelper(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(info.Data()->IsExternal());

  gin::Arguments arguments(info);
  v8::Isolate* isolate = arguments.isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(context,
                                                   kDontCreateIfMissing);
  if (!data)
    return;

  v8::Local<v8::External> external = info.Data().As<v8::External>();
  auto* raw_callback = static_cast<OneTimeMessageCallback*>(external->Value());
  auto iter = base::ranges::find(data->pending_callbacks, raw_callback,
                                 &std::unique_ptr<OneTimeMessageCallback>::get);
  if (iter == data->pending_callbacks.end())
    return;

  std::unique_ptr<OneTimeMessageCallback> callback = std::move(*iter);
  data->pending_callbacks.erase(iter);
  std::move(*callback).Run(&arguments);
}

// Called with the results of dispatching an onMessage event to listeners.
// Returns true if any of the listeners responded with `true`, indicating they
// will respond to the call asynchronously.
bool WillListenerReplyAsync(std::optional<base::Value> result) {
  // `result` can be `nullopt` if the context was destroyed before the
  // listeners were ran (or while they were running).
  if (!result)
    return false;

  if (const base::Value::Dict* dict = result->GetIfDict()) {
    // We expect results in the form of an object with an array of results as
    // a `results` property.
    if (const base::Value::List* list = dict->FindList("results")) {
      // Check if any of the results is `true`.
      for (const base::Value& value : *list) {
        if (value.is_bool() && value.GetBool())
          return true;
      }
    }
  }

  return false;
}

}  // namespace

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
    message_port_host->ClosePort(/*close_channel=*/true);
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
  // listener return `true` if they intend to respond asynchronously; otherwise
  // we close the port.

  auto callback = std::make_unique<OneTimeMessageCallback>(
      base::BindOnce(&OneTimeMessageHandler::OnOneTimeMessageResponse,
                     weak_factory_.GetWeakPtr(), target_port_id));
  v8::Local<v8::External> external = v8::External::New(isolate, callback.get());
  v8::Local<v8::Function> response_function;

  if (!v8::Function::New(context, &OneTimeMessageResponseHelper, external)
           .ToLocal(&response_function)) {
    NOTREACHED_IN_MIGRATION();
    return handled;
  }

  new GCCallback(
      script_context, response_function,
      base::BindOnce(&OneTimeMessageHandler::OnResponseCallbackCollected,
                     weak_factory_.GetWeakPtr(), script_context, target_port_id,
                     reinterpret_cast<CallbackID>(callback.get())),
      base::OnceClosure());

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
    v8::LocalVector<v8::Value> args(isolate,
                                    {v8_message, v8_sender, response_function});

    JSRunner::ResultCallback dispatch_callback;
    // For runtime.onMessage, we require that the listener return `true` if they
    // intend to respond asynchronously. Check the results of the listeners.
    if (port.event_name == messaging_util::kOnMessageEvent) {
      dispatch_callback =
          base::BindOnce(&OneTimeMessageHandler::OnEventFired,
                         weak_factory_.GetWeakPtr(), target_port_id);
    }

    data->pending_callbacks.push_back(std::move(callback));
    bindings_system_->api_system()->event_handler()->FireEventInContext(
        port.event_name, context, &args, nullptr, std::move(dispatch_callback));
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
      opener.request_id, v8::LocalVector<v8::Value>(v8_context->GetIsolate()),
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
  // because the listener did not return `true`), it might be good to surface an
  // error.
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

  std::string error;
  std::unique_ptr<Message> message = messaging_util::MessageFromV8(
      context, value, port_id.serialization_format, &error);
  if (!message) {
    arguments->ThrowTypeError(error);
    return;
  }
  // If the MessagePortHost is still alive return the response. But the listener
  // might be replying after the channel has been closed.
  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  if (auto* message_port_host =
          bindings_system_->messaging_service()->GetMessagePortHostIfExists(
              script_context, port_id)) {
    message_port_host->PostMessage(*message);
    bindings_system_->messaging_service()->CloseMessagePort(
        script_context, port_id, /*close_channel=*/true);
  }
}

void OneTimeMessageHandler::OnResponseCallbackCollected(
    ScriptContext* script_context,
    const PortId& port_id,
    CallbackID raw_callback) {
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

  auto iter = data->receivers.find(port_id);
  // The channel may already be closed (if the receiver replied before the reply
  // callback was collected).
  if (iter == data->receivers.end())
    return;

  data->receivers.erase(iter);

  // Since there is no way to call the callback anymore, we can remove it from
  // the pending callbacks.
  std::erase_if(
      data->pending_callbacks,
      [raw_callback](const std::unique_ptr<OneTimeMessageCallback>& callback) {
        return reinterpret_cast<CallbackID>(callback.get()) == raw_callback;
      });

  // Close the message port. There's no way to send a reply anymore. Don't
  // close the channel because another listener may reply.
  NativeRendererMessagingService* messaging_service =
      bindings_system_->messaging_service();
  messaging_service->CloseMessagePort(script_context, port_id,
                                      /*close_channel=*/false);
}

void OneTimeMessageHandler::OnEventFired(const PortId& port_id,
                                         v8::Local<v8::Context> context,
                                         std::optional<base::Value> result) {
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

  NativeRendererMessagingService* messaging_service =
      bindings_system_->messaging_service();

  if (WillListenerReplyAsync(std::move(result))) {
    // Inform the browser that one of the listeners said they would be replying
    // later and leave the channel open.
    ScriptContext* script_context = GetScriptContextFromV8Context(context);
    if (auto* message_port_host = messaging_service->GetMessagePortHostIfExists(
            script_context, port_id)) {
      message_port_host->ResponsePending();
    }
    return;
  }

  data->receivers.erase(iter);

  // The listener did not reply and did not return `true` from any of its
  // listeners. Close the message port. Don't close the channel because another
  // listener (in a separate context) may reply.
  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  messaging_service->CloseMessagePort(script_context, port_id,
                                      /*close_channel=*/false);
}

}  // namespace extensions
