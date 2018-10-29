// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/process_info_native_handler.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"

namespace extensions {

ProcessInfoNativeHandler::ProcessInfoNativeHandler(
    ScriptContext* context,
    const std::string& extension_id,
    const std::string& context_type,
    bool is_incognito_context,
    bool is_component_extension,
    int manifest_version,
    bool send_request_disabled)
    : ObjectBackedNativeHandler(context),
      extension_id_(extension_id),
      context_type_(context_type),
      is_incognito_context_(is_incognito_context),
      is_component_extension_(is_component_extension),
      manifest_version_(manifest_version),
      send_request_disabled_(send_request_disabled) {}

void ProcessInfoNativeHandler::AddRoutes() {
  RouteHandlerFunction("GetExtensionId",
                       base::Bind(&ProcessInfoNativeHandler::GetExtensionId,
                                  base::Unretained(this)));
  RouteHandlerFunction("GetContextType",
                       base::Bind(&ProcessInfoNativeHandler::GetContextType,
                                  base::Unretained(this)));
  RouteHandlerFunction("InIncognitoContext",
                       base::Bind(&ProcessInfoNativeHandler::InIncognitoContext,
                                  base::Unretained(this)));
  RouteHandlerFunction(
      "IsComponentExtension",
      base::Bind(&ProcessInfoNativeHandler::IsComponentExtension,
                 base::Unretained(this)));
  RouteHandlerFunction("GetManifestVersion",
                       base::Bind(&ProcessInfoNativeHandler::GetManifestVersion,
                                  base::Unretained(this)));
  RouteHandlerFunction(
      "IsSendRequestDisabled",
      base::Bind(&ProcessInfoNativeHandler::IsSendRequestDisabled,
                 base::Unretained(this)));
  RouteHandlerFunction(
      "HasSwitch",
      base::Bind(&ProcessInfoNativeHandler::HasSwitch, base::Unretained(this)));
  RouteHandlerFunction("GetPlatform",
                       base::Bind(&ProcessInfoNativeHandler::GetPlatform,
                                  base::Unretained(this)));
}

void ProcessInfoNativeHandler::GetExtensionId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(),
                                                    extension_id_.c_str(),
                                                    v8::NewStringType::kNormal)
                                .ToLocalChecked());
}

void ProcessInfoNativeHandler::GetContextType(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(args.GetIsolate(), context_type_.c_str(),
                              v8::NewStringType::kInternalized)
          .ToLocalChecked());
}

void ProcessInfoNativeHandler::InIncognitoContext(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(is_incognito_context_);
}

void ProcessInfoNativeHandler::IsComponentExtension(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(is_component_extension_);
}

void ProcessInfoNativeHandler::GetManifestVersion(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(static_cast<int32_t>(manifest_version_));
}

void ProcessInfoNativeHandler::IsSendRequestDisabled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (send_request_disabled_) {
    args.GetReturnValue().Set(
        v8::String::NewFromUtf8(
            args.GetIsolate(),
            "sendRequest and onRequest are obsolete."
            " Please use sendMessage and onMessage instead.",
            v8::NewStringType::kNormal)
            .ToLocalChecked());
  }
}

void ProcessInfoNativeHandler::HasSwitch(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString());
  bool has_switch = base::CommandLine::ForCurrentProcess()->HasSwitch(
      *v8::String::Utf8Value(args.GetIsolate(), args[0]));
  args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), has_switch));
}

void ProcessInfoNativeHandler::GetPlatform(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(0, args.Length());
  args.GetReturnValue().Set(
      gin::StringToSymbol(args.GetIsolate(), binding::GetPlatformString()));
}

}  // namespace extensions
