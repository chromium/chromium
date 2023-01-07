// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_BLINK_INTERFACE_REGISTRY_IMPL_H_
#define CONTENT_RENDERER_MOJO_BLINK_INTERFACE_REGISTRY_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/interface_registry.h"

namespace content {

class BlinkInterfaceRegistryImpl final : public blink::InterfaceRegistry {
 public:
  BlinkInterfaceRegistryImpl(
      base::WeakPtr<service_manager::BinderRegistry> interface_registry,
      base::WeakPtr<blink::AssociatedInterfaceRegistry>
          associated_interface_registry);

  BlinkInterfaceRegistryImpl(const BlinkInterfaceRegistryImpl&) = delete;
  BlinkInterfaceRegistryImpl& operator=(const BlinkInterfaceRegistryImpl&) =
      delete;

  ~BlinkInterfaceRegistryImpl();

  // blink::InterfaceRegistry override.
  void AddInterface(
      const char* name,
      const blink::InterfaceFactory& factory,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void AddAssociatedInterface(
      const char* name,
      const blink::AssociatedInterfaceFactory& factory) override;

 private:
  const base::WeakPtr<service_manager::BinderRegistry> interface_registry_;
  const base::WeakPtr<blink::AssociatedInterfaceRegistry>
      associated_interface_registry_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_BLINK_INTERFACE_REGISTRY_IMPL_H_
