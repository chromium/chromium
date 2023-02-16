// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/messaging_bindings.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "extensions/renderer/gc_callback.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"

namespace extensions {

MessagingBindings::MessagingBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

MessagingBindings::~MessagingBindings() = default;

void MessagingBindings::AddRoutes() {
  // TODO(fsamuel, kalman): Move BindToGC out of messaging natives.
  RouteHandlerFunction("BindToGC",
                       base::BindRepeating(&MessagingBindings::BindToGC,
                                           base::Unretained(this)));
}

void MessagingBindings::BindToGC(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsFunction());

  // Destroys itself when the object is GC'd or context is invalidated.
  new GCCallback(context(), args[0].As<v8::Object>(),
                 args[1].As<v8::Function>(), base::OnceClosure());
}

}  // namespace extensions
