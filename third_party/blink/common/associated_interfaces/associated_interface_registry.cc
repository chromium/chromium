// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

extern "C" void V8RecordReplayAssert(const char* format, ...);

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
  V8RecordReplayAssert("AssociatedInterfaceRegistry::TryBindInterface Start");
  auto it = interfaces_.find(name);
  if (it == interfaces_.end()) {
    V8RecordReplayAssert("AssociatedInterfaceRegistry::TryBindInterface #1");
    return false;
  }
  it->second.Run(std::move(*handle));
  V8RecordReplayAssert("AssociatedInterfaceRegistry::TryBindInterface Done");
  return true;
}

base::WeakPtr<AssociatedInterfaceRegistry>
AssociatedInterfaceRegistry::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace blink
