// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/display_source_custom_bindings.h"

#include <stdint.h>

#include "base/bind.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/web/web_dom_media_stream_track.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {
const char kErrorNotSupported[] = "Not supported";
const char kInvalidStreamArgs[] = "Invalid stream arguments";
const char kSessionAlreadyStarted[] = "The session has been already started";
const char kSessionAlreadyTerminating[] = "The session is already terminating";
const char kSessionNotFound[] = "Session not found";
}  // namespace

DisplaySourceCustomBindings::DisplaySourceCustomBindings(
    ScriptContext* context,
    NativeExtensionBindingsSystem* bindings_system)
    : ObjectBackedNativeHandler(context), bindings_system_(bindings_system) {}

DisplaySourceCustomBindings::~DisplaySourceCustomBindings() {}

void DisplaySourceCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "StartSession", "displaySource",
      base::BindRepeating(&DisplaySourceCustomBindings::StartSession,
                          weak_factory_.GetWeakPtr()));
  RouteHandlerFunction(
      "TerminateSession", "displaySource",
      base::BindRepeating(&DisplaySourceCustomBindings::TerminateSession,
                          weak_factory_.GetWeakPtr()));
}

void DisplaySourceCustomBindings::Invalidate() {
  session_map_.clear();
  weak_factory_.InvalidateWeakPtrs();
  ObjectBackedNativeHandler::Invalidate();
}

namespace {

v8::Local<v8::Value> GetChildValue(v8::Local<v8::Object> value,
                                   const std::string& key_name,
                                   v8::Local<v8::Context> context) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> key;
  v8::Local<v8::Value> child_value;
  if (v8::String::NewFromUtf8(isolate, key_name.c_str(),
                              v8::NewStringType::kNormal)
          .ToLocal(&key) &&
      value->HasOwnProperty(context, key).FromMaybe(false) &&
      value->Get(context, key).ToLocal(&child_value)) {
    return child_value;
  }
  return v8::Null(isolate);
}

int32_t GetCallbackId() {
  static int32_t sCallId = 0;
  return ++sCallId;
}

}  // namespace

void DisplaySourceCustomBindings::StartSession(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsObject());

  v8::Isolate* isolate = context()->isolate();
  v8::Local<v8::Context> v8_context = context()->v8_context();
  v8::Local<v8::Object> start_info = args[0].As<v8::Object>();

  v8::Local<v8::Value> sink_id_val =
      GetChildValue(start_info, "sinkId", v8_context);
  CHECK(sink_id_val->IsInt32());
  const int sink_id = sink_id_val.As<v8::Int32>()->Value();
  if (GetDisplaySession(sink_id)) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kSessionAlreadyStarted,
                                v8::NewStringType::kNormal)
            .ToLocalChecked()));
    return;
  }

  v8::Local<v8::Value> video_stream_val =
      GetChildValue(start_info, "videoTrack", v8_context);
  v8::Local<v8::Value> audio_stream_val =
      GetChildValue(start_info, "audioTrack", v8_context);

  if ((video_stream_val->IsNull() || video_stream_val->IsUndefined()) &&
      (audio_stream_val->IsNull() || audio_stream_val->IsUndefined())) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kInvalidStreamArgs,
                                v8::NewStringType::kNormal)
            .ToLocalChecked()));
    return;
  }

  blink::WebMediaStreamTrack audio_track, video_track;

  if (!video_stream_val->IsNull() && !video_stream_val->IsUndefined()) {
    CHECK(video_stream_val->IsObject());
    video_track = blink::WebDOMMediaStreamTrack::FromV8Value(video_stream_val)
                      .Component();
    if (video_track.IsNull()) {
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(isolate, kInvalidStreamArgs,
                                  v8::NewStringType::kNormal)
              .ToLocalChecked()));
      return;
    }
  }
  if (!audio_stream_val->IsNull() && !audio_stream_val->IsUndefined()) {
    CHECK(audio_stream_val->IsObject());
    audio_track = blink::WebDOMMediaStreamTrack::FromV8Value(audio_stream_val)
                      .Component();
    if (audio_track.IsNull()) {
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8(isolate, kInvalidStreamArgs,
                                  v8::NewStringType::kNormal)
              .ToLocalChecked()));
      return;
    }
  }

  std::unique_ptr<DisplaySourceAuthInfo> auth_info;
  v8::Local<v8::Value> auth_info_v8_val =
      GetChildValue(start_info, "authenticationInfo", v8_context);
  if (!auth_info_v8_val->IsNull()) {
    CHECK(auth_info_v8_val->IsObject());
    std::unique_ptr<base::Value> auth_info_val =
        content::V8ValueConverter::Create()->FromV8Value(
            auth_info_v8_val, context()->v8_context());
    CHECK(auth_info_val);
    auth_info = DisplaySourceAuthInfo::FromValue(*auth_info_val);
  }

  DisplaySourceSessionParams session_params;
  session_params.sink_id = sink_id;
  session_params.video_track = video_track;
  session_params.audio_track = audio_track;
  session_params.render_frame = context()->GetRenderFrame();
  if (auth_info) {
    session_params.auth_method = auth_info->method;
    session_params.auth_data = auth_info->data ? *auth_info->data : "";
  }
  std::unique_ptr<DisplaySourceSession> session =
      DisplaySourceSessionFactory::CreateSession(session_params);
  if (!session) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kErrorNotSupported,
                                v8::NewStringType::kNormal)
            .ToLocalChecked()));
    return;
  }

  auto on_terminated_callback =
      base::Bind(&DisplaySourceCustomBindings::OnSessionTerminated,
                 weak_factory_.GetWeakPtr(), sink_id);
  auto on_error_callback =
      base::Bind(&DisplaySourceCustomBindings::OnSessionError,
                 weak_factory_.GetWeakPtr(), sink_id);
  session->SetNotificationCallbacks(on_terminated_callback, on_error_callback);

  int32_t call_id = GetCallbackId();
  args.GetReturnValue().Set(call_id);

  auto on_call_completed =
      base::Bind(&DisplaySourceCustomBindings::OnSessionStarted,
                 weak_factory_.GetWeakPtr(), sink_id, call_id);
  session->Start(on_call_completed);
  session_map_.insert(std::make_pair(sink_id, std::move(session)));
}

void DisplaySourceCustomBindings::TerminateSession(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());

  v8::Isolate* isolate = context()->isolate();
  int sink_id = args[0].As<v8::Int32>()->Value();
  DisplaySourceSession* session = GetDisplaySession(sink_id);
  if (!session) {
    isolate->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(isolate, kSessionNotFound,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()));
    return;
  }

  DisplaySourceSession::State state = session->state();
  DCHECK_NE(state, DisplaySourceSession::Idle);
  if (state == DisplaySourceSession::Establishing) {
    // 'session started' callback has not yet been invoked.
    // This session is not existing for the user.
    isolate->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(isolate, kSessionNotFound,
                                                     v8::NewStringType::kNormal)
                                 .ToLocalChecked()));
    return;
  }

  if (state == DisplaySourceSession::Terminating) {
    isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(isolate, kSessionAlreadyTerminating,
                                v8::NewStringType::kNormal)
            .ToLocalChecked()));
    return;
  }

  int32_t call_id = GetCallbackId();
  args.GetReturnValue().Set(call_id);

  auto on_call_completed =
      base::Bind(&DisplaySourceCustomBindings::OnCallCompleted,
                 weak_factory_.GetWeakPtr(), call_id);
  // The session will get removed from session_map_ in OnSessionTerminated.
  session->Terminate(on_call_completed);
}

void DisplaySourceCustomBindings::OnCallCompleted(
    int call_id,
    bool success,
    const std::string& error_message) {
  v8::Isolate* isolate = context()->isolate();
  ModuleSystem* module_system = context()->module_system();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());

  v8::Local<v8::Value> callback_args[2];
  callback_args[0] = v8::Integer::New(isolate, call_id);
  if (success)
    callback_args[1] = v8::Null(isolate);
  else
    callback_args[1] = v8::String::NewFromUtf8(isolate, error_message.c_str(),
                                               v8::NewStringType::kNormal)
                           .ToLocalChecked();

  module_system->CallModuleMethodSafe("displaySource", "callCompletionCallback",
                                      2, callback_args);
}

void DisplaySourceCustomBindings::OnSessionStarted(
    int sink_id,
    int call_id,
    bool success,
    const std::string& error_message) {
  CHECK(GetDisplaySession(sink_id));
  if (!success) {
    // Session has failed to start, removing it.
    session_map_.erase(sink_id);
  }
  OnCallCompleted(call_id, success, error_message);
}

void DisplaySourceCustomBindings::DispatchSessionTerminated(int sink_id) const {
  base::ListValue event_args;
  event_args.AppendInteger(sink_id);
  bindings_system_->DispatchEventInContext("displaySource.onSessionTerminated",
                                           &event_args, nullptr, context());
}

void DisplaySourceCustomBindings::DispatchSessionError(
    int sink_id,
    DisplaySourceErrorType type,
    const std::string& message) const {
  api::display_source::ErrorInfo error_info;
  error_info.type = type;
  if (!message.empty())
    error_info.description.reset(new std::string(message));

  base::ListValue event_args;
  event_args.AppendInteger(sink_id);
  event_args.Append(error_info.ToValue());
  bindings_system_->DispatchEventInContext(
      "displaySource.onSessionErrorOccured", &event_args, nullptr, context());
}

DisplaySourceSession* DisplaySourceCustomBindings::GetDisplaySession(
    int sink_id) const {
  auto iter = session_map_.find(sink_id);
  if (iter != session_map_.end())
    return iter->second.get();
  return nullptr;
}

void DisplaySourceCustomBindings::OnSessionTerminated(int sink_id) {
  CHECK(GetDisplaySession(sink_id));
  session_map_.erase(sink_id);
  DispatchSessionTerminated(sink_id);
}

void DisplaySourceCustomBindings::OnSessionError(int sink_id,
                                                 DisplaySourceErrorType type,
                                                 const std::string& message) {
  CHECK(GetDisplaySession(sink_id));
  DispatchSessionError(sink_id, type, message);
}

}  // extensions
