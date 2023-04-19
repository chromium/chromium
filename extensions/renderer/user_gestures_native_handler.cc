// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_gestures_native_handler.h"

#include "base/functional/bind.h"
#include "extensions/renderer/extension_interaction_provider.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

UserGesturesNativeHandler::UserGesturesNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void UserGesturesNativeHandler::AddRoutes() {
  RouteHandlerFunction(
      "IsProcessingUserGesture", "test",
      base::BindRepeating(&UserGesturesNativeHandler::IsProcessingUserGesture,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "RunWithUserGesture", "test",
      base::BindRepeating(
          &UserGesturesNativeHandler::RunWithUserActivationForTest,
          base::Unretained(this)));
}

void UserGesturesNativeHandler::IsProcessingUserGesture(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(
      ExtensionInteractionProvider::HasActiveExtensionInteraction(
          context()->v8_context()));
}

void UserGesturesNativeHandler::RunWithUserActivationForTest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());

  if (context()->web_frame()) {
    context()->web_frame()->NotifyUserActivation(
        blink::mojom::UserActivationNotificationType::kTest);
    context()->SafeCallFunction(v8::Local<v8::Function>::Cast(args[0]), 0,
                                nullptr);
  } else if (context()->IsForServiceWorker()) {
    // Note |scoped_extension_interaction| requires a HandleScope.
    const v8::HandleScope handle_scope(context()->isolate());
    const std::unique_ptr<InteractionProvider::Scope> scoped_interaction =
        ExtensionInteractionProvider::Scope::ForWorker(context()->v8_context());
    context()->SafeCallFunction(v8::Local<v8::Function>::Cast(args[0]), 0,
                                nullptr);
  }
}

}  // namespace extensions
