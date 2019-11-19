// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/one_time_message_handler.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/supports_user_data.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_bindings_system.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/get_per_context_data.h"
#include "extensions/renderer/gc_callback.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/messaging_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"
#include "ipc/ipc_message.h"

namespace extensions {

namespace {

// An opener port in the context; i.e., the caller of runtime.sendMessage.
struct OneTimeOpener {
  int request_id = -1;
  int routing_id = MSG_ROUTING_NONE;
};

// A receiver port in the context; i.e., a listener to runtime.onMessage.
struct OneTimeReceiver {
  int routing_id = MSG_ROUTING_NONE;
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

int RoutingIdForScriptContext(ScriptContext* script_context) {
  content::RenderFrame* render_frame = script_context->GetRenderFrame();
  return render_frame ? render_frame->GetRoutingID() : MSG_ROUTING_NONE;
}

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
  auto iter = std::find_if(
      data->pending_callbacks.begin(), data->pending_callbacks.end(),
      [raw_callback](const std::unique_ptr<OneTimeMessageCallback>& callback) {
        return callback.get() == raw_callback;
      });
  if (iter == data->pending_callbacks.end())
    return;

  std::unique_ptr<OneTimeMessageCallback> callback = std::move(*iter);
  data->pending_callbacks.erase(iter);
  std::move(*callback).Run(&arguments);
}

// Called with the results of dispatching an onMessage event to listeners.
// Returns true if any of the listeners responded with `true`, indicating they
// will respond to the call asynchronously.
bool WillListenerReplyAsync(v8::Local<v8::Context> context,
                            v8::MaybeLocal<v8::Value> maybe_results) {
  v8::Local<v8::Value> results;
  // |maybe_results| can be empty if the context was destroyed before the
  // listeners were ran (or while they were running).
  if (!maybe_results.ToLocal(&results))
    return false;

  if (!results->IsObject())
    return false;

  // Suppress any script errors, but bail out if they happen (in theory, we
  // shouldn't have any).
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch try_catch(isolate);
  // We expect results in the form of an object with an array of results as
  // a `results` property.
  v8::Local<v8::Value> results_property;
  if (!results.As<v8::Object>()
           ->Get(context, gin::StringToSymbol(isolate, "results"))
           .ToLocal(&results_property) ||
      !results_property->IsArray()) {
    return false;
  }

  // Check if any of the results is `true`.
  v8::Local<v8::Array> array = results_property.As<v8::Array>();
  uint32_t length = array->Length();
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> val;
    if (!array->Get(context, i).ToLocal(&val))
      return false;

    if (val->IsTrue())
      return true;
  }

  return false;
}

}  // namespace

OneTimeMessageHandler::OneTimeMessageHandler(
    NativeExtensionBindingsSystem* bindings_system)
    : bindings_system_(bindings_system) {}
OneTimeMessageHandler::~OneTimeMessageHandler() {}

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

void OneTimeMessageHandler::SendMessage(
    ScriptContext* script_context,
    const PortId& new_port_id,
    const MessageTarget& target,
    const std::string& method_name,
    bool include_tls_channel_id,
    const Message& message,
    v8::Local<v8::Function> response_callback) {
  v8::Isolate* isolate = script_context->isolate();
  v8::HandleScope handle_scope(isolate);

  DCHECK(new_port_id.is_opener);
  DCHECK_EQ(script_context->context_id(), new_port_id.context_id);

  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(script_context->v8_context(),
                                                   kCreateIfMissing);
  DCHECK(data);

  bool wants_response = !response_callback.IsEmpty();
  int routing_id = RoutingIdForScriptContext(script_context);
  if (wants_response) {
    int request_id =
        bindings_system_->api_system()->request_handler()->AddPendingRequest(
            script_context->v8_context(), response_callback);
    OneTimeOpener& port = data->openers[new_port_id];
    port.request_id = request_id;
    port.routing_id = routing_id;
  }

  IPCMessageSender* ipc_sender = bindings_system_->GetIPCMessageSender();
  ipc_sender->SendOpenMessageChannel(script_context, new_port_id, target,
                                     method_name, include_tls_channel_id);
  ipc_sender->SendPostMessageToPort(new_port_id, message);

  // If the sender doesn't provide a response callback, we can immediately
  // close the channel. Note: we only do this for extension messages, not
  // native apps.
  // TODO(devlin): This is because of some subtle ordering in the browser side,
  // where closing the channel after sending the message causes things to be
  // destroyed in the wrong order. That would be nice to fix.
  if (!wants_response && target.type != MessageTarget::NATIVE_APP) {
    bool close_channel = true;
    ipc_sender->SendCloseMessagePort(routing_id, new_port_id, close_channel);
  }
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
  receiver.routing_id = RoutingIdForScriptContext(script_context);
  receiver.event_name = event_name;
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
  // callback through which the port can respond. The port stays open until
  // we receive a response.
  // TODO(devlin): With chrome.runtime.sendMessage, we actually require that a
  // listener return `true` if they intend to respond asynchronously; otherwise
  // we close the port.
  auto callback = std::make_unique<OneTimeMessageCallback>(
      base::Bind(&OneTimeMessageHandler::OnOneTimeMessageResponse,
                 weak_factory_.GetWeakPtr(), target_port_id));
  v8::Local<v8::External> external = v8::External::New(isolate, callback.get());
  v8::Local<v8::Function> response_function;

  if (!v8::Function::New(context, &OneTimeMessageResponseHelper, external)
           .ToLocal(&response_function)) {
    NOTREACHED();
    return handled;
  }

  // We shouldn't need to monitor context invalidation here. We store the ports
  // for the context in PerContextData (cleaned up on context destruction), and
  // the browser watches for frame navigation or destruction, and cleans up
  // orphaned channels.
  base::Closure on_context_invalidated;

  new GCCallback(
      script_context, response_function,
      base::Bind(&OneTimeMessageHandler::OnResponseCallbackCollected,
                 weak_factory_.GetWeakPtr(), script_context, target_port_id),
      base::Closure());

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> v8_message =
      messaging_util::MessageToV8(context, message);
  v8::Local<v8::Object> v8_sender = port.sender.Get(isolate);
  std::vector<v8::Local<v8::Value>> args = {v8_message, v8_sender,
                                            response_function};

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
  v8::Local<v8::Value> v8_message =
      messaging_util::MessageToV8(v8_context, message);
  std::vector<v8::Local<v8::Value>> args = {v8_message};
  bindings_system_->api_system()->request_handler()->CompleteRequest(
      port.request_id, args, std::string());

  bool close_channel = true;
  bindings_system_->GetIPCMessageSender()->SendCloseMessagePort(
      port.routing_id, target_port_id, close_channel);

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
  const OneTimeOpener port = iter->second;
  DCHECK_NE(-1, port.request_id);

  // We erase the opener now, since delivering the reply can cause JS to run,
  // which could either invalidate the context or modify the |openers|
  // collection (e.g., by sending another message).
  data->openers.erase(iter);

  bindings_system_->api_system()->request_handler()->CompleteRequest(
      port.request_id, std::vector<v8::Local<v8::Value>>(),
      // If the browser doesn't supply an error message, we supply a generic
      // one.
      error_message.empty()
          ? "The message port closed before a response was received."
          : error_message);

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

  int routing_id = iter->second.routing_id;
  data->receivers.erase(iter);

  v8::Local<v8::Value> value;
  // We allow omitting the message argument (e.g., sendMessage()). Default the
  // value to undefined.
  if (arguments->Length() > 0)
    CHECK(arguments->GetNext(&value));
  else
    value = v8::Undefined(isolate);

  std::string error;
  std::unique_ptr<Message> message =
      messaging_util::MessageFromV8(context, value, &error);
  if (!message) {
    arguments->ThrowTypeError(error);
    return;
  }
  IPCMessageSender* ipc_sender = bindings_system_->GetIPCMessageSender();
  ipc_sender->SendPostMessageToPort(port_id, *message);
  bool close_channel = true;
  ipc_sender->SendCloseMessagePort(routing_id, port_id, close_channel);
}

void OneTimeMessageHandler::OnResponseCallbackCollected(
    ScriptContext* script_context,
    const PortId& port_id) {
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

  int routing_id = iter->second.routing_id;
  data->receivers.erase(iter);

  // Close the message port. There's no way to send a reply anymore. Don't
  // close the channel because another listener may reply.
  IPCMessageSender* ipc_sender = bindings_system_->GetIPCMessageSender();
  bool close_channel = false;
  ipc_sender->SendCloseMessagePort(routing_id, port_id, close_channel);
}

void OneTimeMessageHandler::OnEventFired(const PortId& port_id,
                                         v8::Local<v8::Context> context,
                                         v8::MaybeLocal<v8::Value> result) {
  // The context could be tearing down by the time the event is fully
  // dispatched.
  OneTimeMessageContextData* data =
      GetPerContextData<OneTimeMessageContextData>(context,
                                                   kDontCreateIfMissing);
  if (!data)
    return;

  if (WillListenerReplyAsync(context, result))
    return;  // The listener will reply later; leave the channel open.

  auto iter = data->receivers.find(port_id);
  // The channel may already be closed (if the listener replied).
  if (iter == data->receivers.end())
    return;

  int routing_id = iter->second.routing_id;
  data->receivers.erase(iter);

  // The listener did not reply and did not return `true` from any of its
  // listeners. Close the message port. Don't close the channel because another
  // listener (in a separate context) may reply.
  IPCMessageSender* ipc_sender = bindings_system_->GetIPCMessageSender();
  bool close_channel = false;
  ipc_sender->SendCloseMessagePort(routing_id, port_id, close_channel);
}

}  // namespace extensions
