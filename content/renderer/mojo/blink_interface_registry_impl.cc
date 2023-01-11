// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/blink_interface_registry_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {

BlinkInterfaceRegistryImpl::BlinkInterfaceRegistryImpl(
    base::WeakPtr<service_manager::BinderRegistry> interface_registry,
    base::WeakPtr<blink::AssociatedInterfaceRegistry>
        associated_interface_registry)
    : interface_registry_(interface_registry),
      associated_interface_registry_(associated_interface_registry) {}

BlinkInterfaceRegistryImpl::~BlinkInterfaceRegistryImpl() = default;

void BlinkInterfaceRegistryImpl::AddInterface(
    const char* name,
    const blink::InterfaceFactory& factory,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!interface_registry_)
    return;

  interface_registry_->AddInterface(name, factory, std::move(task_runner));
}

void BlinkInterfaceRegistryImpl::AddAssociatedInterface(
    const char* name,
    const blink::AssociatedInterfaceFactory& factory) {
  if (!associated_interface_registry_)
    return;

  associated_interface_registry_->AddInterface(name, factory);
}

}  // namespace content
