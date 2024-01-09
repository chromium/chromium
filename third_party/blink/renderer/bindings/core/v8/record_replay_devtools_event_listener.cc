// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/record_replay_devtools_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/custom_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

static v8::Local<v8::String> ToV8String(v8::Isolate* isolate, const char* value) {
  return v8::String::NewFromUtf8(isolate, value,
                                 v8::NewStringType::kInternalized).ToLocalChecked();
}
static const std::string V8ToString(v8::Isolate* isolate, v8::Local<v8::Value> str) {
  v8::String::Utf8Value s(isolate, str);
  return *s;
}

static bool GetStringProperty(v8::Local<v8::Context> context, v8::Local<v8::Object> obj, const char* name, v8::Local<v8::String>* out) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> v8Name = ToV8String(isolate, name);
  v8::Local<v8::Value> v8Value = obj->Get(context, v8Name).ToLocalChecked();

  return v8Value->ToString(context).ToLocal(out);
}

static bool GetObjectProperty(v8::Local<v8::Context> context, v8::Local<v8::Object> obj, const char* name, v8::Local<v8::Object>* out) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::String> v8Name = ToV8String(isolate, name);
  v8::Local<v8::Value> v8Value = obj->Get(context, v8Name).ToLocalChecked();

  return v8Value->ToObject(context).ToLocal(out);
}

static bool StringEquals(v8::Isolate* isolate, v8::Local<v8::String> str1, const char* str2) {
  return str1->StringEquals(ToV8String(isolate, str2));
}


void RecordReplayDevtoolsEventListener::Invoke(ExecutionContext* context, Event* event) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();
  ScriptState* scriptState = ScriptState::Current(isolate);
  CustomEvent* customEvent = To<CustomEvent>(event);

  if (!customEvent) {
    return;
  }

  v8::Local<v8::Value> detail = customEvent->detail(scriptState).V8Value();
  v8::Local<v8::String> detail_json;
  if (!detail->ToString(v8_context).ToLocal(&detail_json)) {
    LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: detail is not a string";
    return;
  }

  // for debugging:
  // LOG(ERROR) << "RecordReplayDevtoolsEventListener: detail = " << V8ToString(isolate, detail_json);

  // detail is a JSON stringified object with one of the following forms:

  // { "id": "record-replay-token", "message": { "type": "connect" } }      => register auth token observer
  // { "id": "record-replay-token", "message": { "type": "login" } }        => open external browser to login
  // { "id": "record-replay-token", "message": { "token": <string|null> } } => set access token if string.  clear if null (or undefined?)
  // { "id": "record-replay", "message": { "user": <string|null> } }        => set user if string.  clear if null (or undefined?)

  v8::Local<v8::Object> detail_obj;
  if (!v8::JSON::Parse(v8_context, detail_json).ToLocalChecked()->ToObject(v8_context).ToLocal(&detail_obj)) {
    LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: detail is not a JSON object";
    return;
  }

  // always pull out the id and message properties, and early out if id isn't a string or message isn't an object
  v8::Local<v8::String> id_str;
  if (!GetStringProperty(v8_context, detail_obj, "id", &id_str)) {
    LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: id is not an string";
    return;
  }

  v8::Local<v8::Object> message_obj;
  if (!GetObjectProperty(v8_context, detail_obj, "message", &message_obj)) {
    LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: message is not an object";
    return;
  }


  if (StringEquals(isolate, id_str, "record-replay-token")) {
    HandleRecordReplayTokenMessage(v8_context, message_obj);
  } else if (StringEquals(isolate, id_str, "record-replay")) {
    HandleRecordReplayMessage(v8_context, message_obj);
  } else {
    LOG(ERROR) << "[RUN-2863] Unknown event id: " << V8ToString(isolate, id_str);
  }
}

void RecordReplayDevtoolsEventListener::HandleRecordReplayTokenMessage(v8::Local<v8::Context> context, v8::Local<v8::Object> message) {
  v8::Isolate* isolate = context->GetIsolate();

  // cases here:
  // { "id": "record-replay-token", "message": { "type": "connect" } }      => register auth token observer
  // { "id": "record-replay-token", "message": { "type": "login" } }        => open external browser to login
  // { "id": "record-replay-token", "message": { "token": <string|null> } } => set access token if string.  clear if null (or undefined?)

  // first check if there's a type property to handle the first two cases above.
  v8::Local<v8::Value> message_type = message->Get(context, ToV8String(isolate, "type")).ToLocalChecked();
  if (message_type->IsString()) {
    // message is either `{ type: "connect" }` or `{ type: "login" }`, with neither payload carrying additional info.
    if (StringEquals(isolate, message_type.As<v8::String>(), "connect")) {
      LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: connect message received";
      local_frame_->RecordReplayRegisterAuthTokenObserver();
      return;
    }

    if (StringEquals(isolate, message_type.As<v8::String>(), "login")) {
      LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: login message received";
      local_frame_->RecordReplayLogin();
      return;
    }

    LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: unknown record-replay-token message type: " << V8ToString(isolate, message_type);
  }

  // if we're here, we should only be in the `{ token: ... }` case from the list above.
  v8::Local<v8::Value> message_token = message->Get(context, ToV8String(isolate, "token")).ToLocalChecked();
  if (message_token->IsString()) {
    LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: set access token message received, token = " << V8ToString(isolate, message_token);
    local_frame_->RecordReplaySetToken(ToCoreString(message_token.As<v8::String>()));
    return;
  }

  if (message_token->IsNullOrUndefined()) {
    LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: clear access token message received";
    local_frame_->RecordReplayClearToken();
    return;
  }

  LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: unknown record-replay-token message";
}

void RecordReplayDevtoolsEventListener::HandleRecordReplayMessage(v8::Local<v8::Context> context, v8::Local<v8::Object> message) {
  v8::Isolate* isolate = context->GetIsolate();

  // the only message handled here is `{ user: <string|null> }`
  v8::Local<v8::Value> message_user = message->Get(context, ToV8String(isolate, "user")).ToLocalChecked();
  if (message_user->IsString()) {
    LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: set user message received, user = " << V8ToString(isolate, message_user);
    local_frame_->RecordReplaySetUser(ToCoreString(message_user.As<v8::String>()));
    return;
  }

  if (message_user->IsNullOrUndefined()) {
    LOG(ERROR) << "[RUN-2863] RecordReplayDevtoolsEventListener: clear user message received";
    local_frame_->RecordReplayClearUser();
    return;
  }

  LOG(ERROR) << "[RUN-2863] Unknown record-replay message type";
  return;
}

void RecordReplayDevtoolsEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(local_frame_);
  EventListener::Trace(visitor);
}

} // namespace blink