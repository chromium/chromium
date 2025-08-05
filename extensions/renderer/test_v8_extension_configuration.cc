// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/test_v8_extension_configuration.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "extensions/renderer/safe_builtins.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-extension.h"

namespace extensions {

TestV8ExtensionConfiguration::TestV8ExtensionConfiguration()
    : v8_extension_configuration_(
          new v8::ExtensionConfiguration(1, &v8_extension_name_)) {
  auto safe_builtins = SafeBuiltins::CreateV8Extension();
  v8_extension_name_ = safe_builtins->name();
  v8::RegisterExtension(std::move(safe_builtins));
}

TestV8ExtensionConfiguration::~TestV8ExtensionConfiguration() = default;

// static
v8::ExtensionConfiguration* TestV8ExtensionConfiguration::GetConfiguration() {
  static base::NoDestructor<TestV8ExtensionConfiguration> config;
  return config->v8_extension_configuration_.get();
}

}  // namespace extensions
