// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/interface_registry.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {
namespace {

class EmptyInterfaceRegistry : public InterfaceRegistry {
  void AddInterface(
      const char* name,
      const InterfaceFactory& factory,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {}
  void AddAssociatedInterface(
      const char* name,
      const AssociatedInterfaceFactory& factory) override {}
};

}  // namespace

InterfaceRegistry* InterfaceRegistry::GetEmptyInterfaceRegistry() {
  DEFINE_STATIC_LOCAL(EmptyInterfaceRegistry, empty_interface_registry, ());
  return &empty_interface_registry;
}

}  // namespace blink
