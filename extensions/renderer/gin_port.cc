// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/gin_port.h"

#include <cstring>
#include <vector>

#include "base/bind.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "extensions/renderer/bindings/event_emitter.h"
#include "extensions/renderer/messaging_util.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"

namespace extensions {

namespace {

constexpr char kSenderKey[] = "sender";
constexpr char kOnMessageEvent[] = "onMessage";
constexpr char kOnDisconnectEvent[] = "onDisconnect";
constexpr char kContextInvalidatedError[] = "Extension context invalidated.";

}  // namespace

GinPort::GinPort(v8::Local<v8::Context> context,
                 const PortId& port_id,
                 int routing_id,
                 const std::string& name,
                 APIEventHandler* event_handler,
                 Delegate* delegate)
    : port_id_(port_id),
      routing_id_(routing_id),
      name_(name),
      event_handler_(event_handler),
      delegate_(delegate),
      accessed_sender_(false) {
  context_invalidation_listener_.emplace(
      context, base::BindOnce(&GinPort::OnContextInvalidated,
                              weak_factory_.GetWeakPtr()));
}

GinPort::~GinPort() {}

gin::WrapperInfo GinPort::kWrapperInfo = {gin::kEmbedderNativeGin};

gin::ObjectTemplateBuilder GinPort::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<GinPort>::GetObjectTemplateBuilder(isolate)
      .SetMethod("disconnect", &GinPort::DisconnectHandler)
      .SetMethod("postMessage", &GinPort::PostMessageHandler)
      .SetLazyDataProperty("name", &GinPort::GetName)
      .SetLazyDataProperty("onDisconnect", &GinPort::GetOnDisconnectEvent)
      .SetLazyDataProperty("onMessage", &GinPort::GetOnMessageEvent)
      .SetLazyDataProperty("sender", &GinPort::GetSender);
}

const char* GinPort::GetTypeName() {
  return "Port";
}

void GinPort::DispatchOnMessage(v8::Local<v8::Context> context,
                                const Message& message) {
  DCHECK_EQ(kActive, state_);

  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Value> parsed_message =
      messaging_util::MessageToV8(context, message);
  if (parsed_message.IsEmpty()) {
    NOTREACHED();
    return;
  }

  v8::Local<v8::Object> self = GetWrapper(isolate).ToLocalChecked();
  std::vector<v8::Local<v8::Value>> args = {parsed_message, self};
  DispatchEvent(context, &args, kOnMessageEvent);
}

void GinPort::DispatchOnDisconnect(v8::Local<v8::Context> context) {
  DCHECK_EQ(kActive, state_);

  // Update |state_| before dispatching the onDisconnect event, so that we are
  // able to reject attempts to disconnect the port again or to send a message
  // from the event handler.
  state_ = kDisconnected;

  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> self = GetWrapper(isolate).ToLocalChecked();
  std::vector<v8::Local<v8::Value>> args = {self};
  DispatchEvent(context, &args, kOnDisconnectEvent);

  InvalidateEvents(context);

  DCHECK_NE(state_, kActive);
}

void GinPort::SetSender(v8::Local<v8::Context> context,
                        v8::Local<v8::Value> sender) {
  DCHECK_EQ(kActive, state_);
  DCHECK(!accessed_sender_)
      << "|sender| can only be set before its first access.";

  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Private> key =
      v8::Private::ForApi(isolate, gin::StringToSymbol(isolate, kSenderKey));
  v8::Maybe<bool> set_result = wrapper->SetPrivate(context, key, sender);
  DCHECK(set_result.IsJust() && set_result.FromJust());
}

void GinPort::DisconnectHandler(gin::Arguments* arguments) {
  if (state_ == kInvalidated) {
    ThrowError(arguments->isolate(), kContextInvalidatedError);
    return;
  }

  // NOTE: We don't currently throw an error for calling disconnect() multiple
  // times, but we could.
  if (state_ == kDisconnected)
    return;

  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();
  InvalidateEvents(context);
  delegate_->ClosePort(context, port_id_, routing_id_);
  state_ = kDisconnected;
}

void GinPort::PostMessageHandler(gin::Arguments* arguments,
                                 v8::Local<v8::Value> v8_message) {
  v8::Isolate* isolate = arguments->isolate();
  v8::Local<v8::Context> context = arguments->GetHolderCreationContext();

  if (state_ == kInvalidated) {
    ThrowError(isolate, kContextInvalidatedError);
    return;
  }

  if (state_ == kDisconnected) {
    ThrowError(isolate, "Attempting to use a disconnected port object");
    return;
  }

  std::string error;
  std::unique_ptr<Message> message =
      messaging_util::MessageFromV8(context, v8_message, &error);
  // NOTE(devlin): JS-based bindings just log to the console here and return,
  // rather than throwing an error. But it really seems like it should be an
  // error. Let's see how this goes.
  if (!message) {
    ThrowError(isolate, error);
    return;
  }

  delegate_->PostMessageToPort(context, port_id_, routing_id_,
                               std::move(message));
}

std::string GinPort::GetName() {
  return name_;
}

v8::Local<v8::Value> GinPort::GetOnDisconnectEvent(gin::Arguments* arguments) {
  return GetEvent(arguments->GetHolderCreationContext(), kOnDisconnectEvent);
}

v8::Local<v8::Value> GinPort::GetOnMessageEvent(gin::Arguments* arguments) {
  return GetEvent(arguments->GetHolderCreationContext(), kOnMessageEvent);
}

v8::Local<v8::Value> GinPort::GetSender(gin::Arguments* arguments) {
  accessed_sender_ = true;
  v8::Isolate* isolate = arguments->isolate();
  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Private> key =
      v8::Private::ForApi(isolate, gin::StringToSymbol(isolate, kSenderKey));
  v8::Local<v8::Value> sender;
  if (!wrapper->GetPrivate(arguments->GetHolderCreationContext(), key)
           .ToLocal(&sender)) {
    NOTREACHED();
    return v8::Undefined(isolate);
  }

  return sender;
}

v8::Local<v8::Object> GinPort::GetEvent(v8::Local<v8::Context> context,
                                        base::StringPiece event_name) {
  DCHECK(event_name == kOnMessageEvent || event_name == kOnDisconnectEvent);
  v8::Isolate* isolate = context->GetIsolate();

  if (state_ == kInvalidated) {
    ThrowError(isolate, kContextInvalidatedError);
    return v8::Local<v8::Object>();
  }

  v8::Local<v8::Object> wrapper = GetWrapper(isolate).ToLocalChecked();
  v8::Local<v8::Private> key =
      v8::Private::ForApi(isolate, gin::StringToSymbol(isolate, event_name));
  v8::Local<v8::Value> event_val;
  if (!wrapper->GetPrivate(context, key).ToLocal(&event_val)) {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

  DCHECK(!event_val.IsEmpty());
  v8::Local<v8::Object> event_object;
  if (event_val->IsUndefined()) {
    event_object = event_handler_->CreateAnonymousEventInstance(context);
    v8::Maybe<bool> set_result =
        wrapper->SetPrivate(context, key, event_object);
    if (!set_result.IsJust() || !set_result.FromJust()) {
      NOTREACHED();
      return v8::Local<v8::Object>();
    }
  } else {
    event_object = event_val.As<v8::Object>();
  }
  return event_object;
}

void GinPort::DispatchEvent(v8::Local<v8::Context> context,
                            std::vector<v8::Local<v8::Value>>* args,
                            base::StringPiece event_name) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Value> on_message = GetEvent(context, event_name);
  EventEmitter* emitter = nullptr;
  gin::Converter<EventEmitter*>::FromV8(isolate, on_message, &emitter);
  CHECK(emitter);

  emitter->Fire(context, args, nullptr, JSRunner::ResultCallback());
}

void GinPort::OnContextInvalidated() {
  DCHECK_NE(state_, kInvalidated);
  state_ = kInvalidated;
  // Note: no need to InvalidateEvents() here, since the APIEventHandler will
  // invalidate them when the context is disposed.
}

void GinPort::InvalidateEvents(v8::Local<v8::Context> context) {
  // No need to invalidate the events if the context itself was already
  // invalidated; the APIEventHandler will have already cleaned up the
  // listeners.
  if (state_ == kInvalidated)
    return;

  // TODO(devlin): By calling GetEvent() here, we'll end up creating an event
  // if one didn't exist. It would be more efficient to only invalidate events
  // that the port has already created.
  event_handler_->InvalidateCustomEvent(context,
                                        GetEvent(context, kOnMessageEvent));
  event_handler_->InvalidateCustomEvent(context,
                                        GetEvent(context, kOnDisconnectEvent));
}

void GinPort::ThrowError(v8::Isolate* isolate, base::StringPiece error) {
  isolate->ThrowException(
      v8::Exception::Error(gin::StringToV8(isolate, error)));
}

}  // namespace extensions
