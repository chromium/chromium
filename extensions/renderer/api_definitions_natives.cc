// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api_definitions_natives.h"

#include "base/functional/bind.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_schema_registry.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-function-callback.h"

namespace extensions {

ApiDefinitionsNatives::ApiDefinitionsNatives(
    V8SchemaRegistry* v8_schema_registry,
    ScriptContext* context)
    : ObjectBackedNativeHandler(context),
      v8_schema_registry_(v8_schema_registry) {}

void ApiDefinitionsNatives::AddRoutes() {
  RouteHandlerFunction(
      "GetExtensionAPIDefinitionsForTest", "test",
      base::BindRepeating(
          &ApiDefinitionsNatives::GetExtensionAPIDefinitionsForTest,
          base::Unretained(this)));
}

void ApiDefinitionsNatives::GetExtensionAPIDefinitionsForTest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  std::vector<std::string> apis;
  const FeatureProvider* feature_provider = FeatureProvider::GetAPIFeatures();
  for (const auto& map_entry : feature_provider->GetAllFeatures()) {
    if (!feature_provider->GetParent(*map_entry.second) &&
        context()->GetAvailability(map_entry.first).is_available()) {
      apis.push_back(map_entry.first);
    }
  }
  args.GetReturnValue().Set(
      v8_schema_registry_->GetSchemas(args.GetIsolate(), apis));
}

}  // namespace extensions
