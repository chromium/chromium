// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/id_generator_custom_bindings.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8-function-callback.h"

namespace extensions {

IdGeneratorCustomBindings::IdGeneratorCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void IdGeneratorCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetNextId", base::BindRepeating(&IdGeneratorCustomBindings::GetNextId,
                                       base::Unretained(this)));
  RouteHandlerFunction(
      "GetNextScopedId",
      base::BindRepeating(&IdGeneratorCustomBindings::GetNextScopedId,
                          base::Unretained(this)));
}

void IdGeneratorCustomBindings::GetNextId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Make sure 0 is never returned because some APIs (particularly WebRequest)
  // have special meaning for 0 IDs.
  static int32_t next_id = 1;
  args.GetReturnValue().Set(next_id++);
}

void IdGeneratorCustomBindings::GetNextScopedId(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(context()->GetNextIdFromCounter());
}

}  // namespace extensions
