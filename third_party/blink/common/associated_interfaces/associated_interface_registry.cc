// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace blink {

AssociatedInterfaceRegistry::AssociatedInterfaceRegistry() = default;

AssociatedInterfaceRegistry::~AssociatedInterfaceRegistry() = default;

void AssociatedInterfaceRegistry::AddInterface(const std::string& name,
                                               const Binder& binder) {
  auto result = interfaces_.emplace(name, binder);
  DCHECK(result.second);
}

void AssociatedInterfaceRegistry::RemoveInterface(const std::string& name) {
  interfaces_.erase(name);
}

bool AssociatedInterfaceRegistry::TryBindInterface(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  auto it = interfaces_.find(name);
  if (it == interfaces_.end())
    return false;
  it->second.Run(std::move(*handle));
  return true;
}

base::WeakPtr<AssociatedInterfaceRegistry>
AssociatedInterfaceRegistry::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace blink
