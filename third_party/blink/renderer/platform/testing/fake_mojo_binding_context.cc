// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/fake_mojo_binding_context.h"

#include <utility>

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace blink {

FakeMojoBindingContext::FakeMojoBindingContext(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

const BrowserInterfaceBrokerProxy&
FakeMojoBindingContext::GetBrowserInterfaceBroker() const {
  return GetEmptyBrowserInterfaceBroker();
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeMojoBindingContext::GetTaskRunner(TaskType) {
  return task_runner_;
}

void FakeMojoBindingContext::Dispose() {
  if (!IsContextDestroyed()) {
    NotifyContextDestroyed();
  }
}

}  // namespace blink
