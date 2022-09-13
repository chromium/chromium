// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SERVICE_WORKER_NATIVES_H_
#define EXTENSIONS_RENDERER_SERVICE_WORKER_NATIVES_H_

#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

class ServiceWorkerNatives : public ObjectBackedNativeHandler {
 public:
  explicit ServiceWorkerNatives(ScriptContext* context);
  ServiceWorkerNatives(const ServiceWorkerNatives&) = delete;
  ServiceWorkerNatives& operator=(const ServiceWorkerNatives&) = delete;
  ~ServiceWorkerNatives() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  // Returns (via v8) whether the associated ScriptContext is for a service
  // worker.
  // TODO(devlin): We have a similar method in SetIconNatives; combine them.
  void IsServiceWorkerContext(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SERVICE_WORKER_NATIVES_H_
