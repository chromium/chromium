// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/messaging_bindings.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "extensions/renderer/gc_callback.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8.h"

namespace extensions {

MessagingBindings::MessagingBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

MessagingBindings::~MessagingBindings() {}

void MessagingBindings::AddRoutes() {
  // TODO(fsamuel, kalman): Move BindToGC out of messaging natives.
  RouteHandlerFunction("BindToGC",
                       base::BindRepeating(&MessagingBindings::BindToGC,
                                           base::Unretained(this)));
}

void MessagingBindings::BindToGC(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 3);
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsFunction());
  CHECK(args[2]->IsInt32());
  // TODO(devlin): Update callers to not pass a port ID.
  // int js_port_id = args[2].As<v8::Int32>()->Value();
  base::Closure fallback = base::DoNothing();
  // Destroys itself when the object is GC'd or context is invalidated.
  new GCCallback(context(), args[0].As<v8::Object>(),
                 args[1].As<v8::Function>(), fallback);
}

}  // namespace extensions
