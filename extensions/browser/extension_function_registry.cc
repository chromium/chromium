// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function_registry.h"

#include "base/no_destructor.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extensions_browser_client.h"

// static
ExtensionFunctionRegistry& ExtensionFunctionRegistry::GetInstance() {
  static base::NoDestructor<ExtensionFunctionRegistry> instance;
  return *instance;
}

ExtensionFunctionRegistry::ExtensionFunctionRegistry() {
  extensions::ExtensionsBrowserClient* client =
      extensions::ExtensionsBrowserClient::Get();
  if (client) {
    client->RegisterExtensionFunctions(this);
  }
}

ExtensionFunctionRegistry::~ExtensionFunctionRegistry() = default;

bool ExtensionFunctionRegistry::OverrideFunctionForTesting(
    const std::string& name,
    ExtensionFunctionFactory factory) {
  auto iter = factories_.find(name);
  if (iter == factories_.end()) {
    return false;
  }
  iter->second.factory_ = factory;
  return true;
}

scoped_refptr<ExtensionFunction> ExtensionFunctionRegistry::NewFunction(
    const std::string& name) {
  auto iter = factories_.find(name);
  if (iter == factories_.end()) {
    return nullptr;
  }
  scoped_refptr<ExtensionFunction> function = iter->second.factory_();
  function->SetName(iter->second.function_name_);
  function->set_histogram_value(iter->second.histogram_value_);
  return function;
}

void ExtensionFunctionRegistry::Register(const FactoryEntry& entry) {
  factories_[entry.function_name_] = entry;
}

ExtensionFunctionRegistry::FactoryEntry::FactoryEntry() = default;
