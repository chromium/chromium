// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_
#define EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_

#include <memory>

#include "extensions/common/activation_sequence.h"
#include "extensions/renderer/v8_schema_registry.h"

namespace extensions {
class NativeExtensionBindingsSystem;
class ScriptContext;

// Per ServiceWorker data in worker thread.
// TODO(lazyboy): Also put worker ScriptContexts in this.
class ServiceWorkerData {
 public:
  ServiceWorkerData(
      int64_t service_worker_version_id,
      ActivationSequence activation_sequence,
      ScriptContext* context,
      std::unique_ptr<NativeExtensionBindingsSystem> bindings_system);

  ServiceWorkerData(const ServiceWorkerData&) = delete;
  ServiceWorkerData& operator=(const ServiceWorkerData&) = delete;

  ~ServiceWorkerData();

  V8SchemaRegistry* v8_schema_registry() { return v8_schema_registry_.get(); }
  NativeExtensionBindingsSystem* bindings_system() {
    return bindings_system_.get();
  }
  int64_t service_worker_version_id() const {
    return service_worker_version_id_;
  }
  ActivationSequence activation_sequence() const {
    return activation_sequence_;
  }
  ScriptContext* context() const { return context_; }

 private:
  const int64_t service_worker_version_id_;
  const ActivationSequence activation_sequence_;
  ScriptContext* const context_ = nullptr;

  std::unique_ptr<V8SchemaRegistry> v8_schema_registry_;
  std::unique_ptr<NativeExtensionBindingsSystem> bindings_system_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_
