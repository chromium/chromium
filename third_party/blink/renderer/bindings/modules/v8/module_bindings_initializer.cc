// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/module_bindings_initializer.h"

#include "third_party/blink/renderer/bindings/modules/v8/init_idl_interfaces.h"
#include "third_party/blink/renderer/bindings/modules/v8/properties_per_feature_installer.h"
#include "third_party/blink/renderer/bindings/modules/v8/serialization/serialized_script_value_for_modules_factory.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_context_snapshot_impl.h"
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"

namespace blink {

void ModuleBindingsInitializer::Init() {
  bindings::InitIDLInterfaces();
  auto* old_installer =
      SetInstallPropertiesPerFeatureFunc(bindings::InstallPropertiesPerFeature);
  CHECK(!old_installer);
  V8ContextSnapshotImpl::Init();
  SerializedScriptValueFactory::Initialize(
      new SerializedScriptValueForModulesFactory);
}

}  // namespace blink
