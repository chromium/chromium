// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_type_reference_map.h"

#include <ostream>

#include "base/containers/contains.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/argument_spec.h"

namespace extensions {

APITypeReferenceMap::APITypeReferenceMap(InitializeTypeCallback initialize_type)
    : initialize_type_(std::move(initialize_type)) {}
APITypeReferenceMap::~APITypeReferenceMap() = default;

void APITypeReferenceMap::AddSpec(const std::string& name,
                                  std::unique_ptr<ArgumentSpec> spec) {
  DCHECK(!base::Contains(type_refs_, name));
  type_refs_[name] = std::move(spec);
}

const ArgumentSpec* APITypeReferenceMap::GetSpec(
    const std::string& name) const {
  auto iter = type_refs_.find(name);
  if (iter == type_refs_.end()) {
    initialize_type_.Run(name);
    iter = type_refs_.find(name);
  }
  return iter == type_refs_.end() ? nullptr : iter->second.get();
}

void APITypeReferenceMap::AddAPIMethodSignature(
    const std::string& name,
    std::unique_ptr<APISignature> signature) {
  DCHECK(!base::Contains(api_methods_, name))
      << "Cannot re-register signature for: " << name;
  api_methods_[name] = std::move(signature);
}

const APISignature* APITypeReferenceMap::GetAPIMethodSignature(
    const std::string& name) const {
  auto iter = api_methods_.find(name);
  if (iter == api_methods_.end()) {
    initialize_type_.Run(name);
    iter = api_methods_.find(name);
  }
  return iter == api_methods_.end() ? nullptr : iter->second.get();
}

void APITypeReferenceMap::AddTypeMethodSignature(
    const std::string& name,
    std::unique_ptr<APISignature> signature) {
  DCHECK(!base::Contains(type_methods_, name))
      << "Cannot re-register signature for: " << name;
  type_methods_[name] = std::move(signature);
}

const APISignature* APITypeReferenceMap::GetTypeMethodSignature(
    const std::string& name) const {
  auto iter = type_methods_.find(name);
  if (iter == type_methods_.end()) {
    // Find the type name by stripping away the method suffix.
    std::string::size_type dot = name.rfind('.');
    DCHECK_NE(std::string::npos, dot);
    DCHECK_LT(dot, name.size() - 1);
    std::string type_name = name.substr(0, dot);
    initialize_type_.Run(type_name);
    iter = type_methods_.find(name);
  }
  return iter == type_methods_.end() ? nullptr : iter->second.get();
}

bool APITypeReferenceMap::HasTypeMethodSignature(
    const std::string& name) const {
  return base::Contains(type_methods_, name);
}

const APISignature* APITypeReferenceMap::GetAsyncResponseSignature(
    const std::string& name) const {
  auto iter = api_methods_.find(name);
  return iter == api_methods_.end() ? nullptr : iter->second.get();
}

void APITypeReferenceMap::AddCustomSignature(
    const std::string& name,
    std::unique_ptr<APISignature> signature) {
  DCHECK(!base::Contains(custom_signatures_, name))
      << "Cannot re-register signature for: " << name;
  custom_signatures_[name] = std::move(signature);
}

const APISignature* APITypeReferenceMap::GetCustomSignature(
    const std::string& name) const {
  auto iter = custom_signatures_.find(name);
  return iter != custom_signatures_.end() ? iter->second.get() : nullptr;
}

void APITypeReferenceMap::AddEventSignature(
    const std::string& event_name,
    std::unique_ptr<APISignature> signature) {
  DCHECK(!base::Contains(event_signatures_, event_name))
      << "Cannot re-register signature for: " << event_name;
  event_signatures_[event_name] = std::move(signature);
}

const APISignature* APITypeReferenceMap::GetEventSignature(
    const std::string& event_name) const {
  auto iter = event_signatures_.find(event_name);
  return iter != event_signatures_.end() ? iter->second.get() : nullptr;
}

}  // namespace extensions
