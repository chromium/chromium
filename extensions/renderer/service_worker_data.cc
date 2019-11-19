// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_data.h"

#include "extensions/renderer/native_extension_bindings_system.h"

namespace extensions {

ServiceWorkerData::ServiceWorkerData(
    int64_t service_worker_version_id,
    ScriptContext* context,
    std::unique_ptr<NativeExtensionBindingsSystem> bindings_system)
    : service_worker_version_id_(service_worker_version_id),
      context_(context),
      v8_schema_registry_(new V8SchemaRegistry),
      bindings_system_(std::move(bindings_system)) {}

ServiceWorkerData::~ServiceWorkerData() {}

}  // namespace extensions
