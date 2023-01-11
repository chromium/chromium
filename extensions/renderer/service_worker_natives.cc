// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_natives.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8-function-callback.h"

namespace extensions {

ServiceWorkerNatives::ServiceWorkerNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}
ServiceWorkerNatives::~ServiceWorkerNatives() = default;

void ServiceWorkerNatives::AddRoutes() {
  RouteHandlerFunction(
      "IsServiceWorkerContext",
      base::BindRepeating(&ServiceWorkerNatives::IsServiceWorkerContext,
                          base::Unretained(this)));
}

void ServiceWorkerNatives::IsServiceWorkerContext(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(context()->IsForServiceWorker());
}

}  // namespace extensions
