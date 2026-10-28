// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/in_renderer_tensor_handle.h"

#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace webnn {

InRendererTensorHandle::InRendererTensorHandle(
    scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
    : base::RefCountedDeleteOnSequence<InRendererTensorHandle>(
          std::move(owning_task_runner)) {}
InRendererTensorHandle::~InRendererTensorHandle() = default;

namespace {

// Guards `Storage()` below. Contention is negligible: register/unregister
// happen once per tensor lifetime, and Get() runs a handful of times per
// tensor from Blink's main thread.
struct RegistryStorage {
  base::Lock lock;
  base::flat_map<blink::WebNNTensorToken, scoped_refptr<InRendererTensorHandle>>
      entries GUARDED_BY(lock);
};

RegistryStorage& Storage() {
  static base::NoDestructor<RegistryStorage> instance;
  return *instance;
}

}  // namespace

// static
void InRendererTensorRegistry::Register(
    const blink::WebNNTensorToken& token,
    scoped_refptr<InRendererTensorHandle> handle) {
  CHECK(handle);
  RegistryStorage& s = Storage();
  base::AutoLock hold(s.lock);
  auto [_, inserted] = s.entries.emplace(token, std::move(handle));
  CHECK(inserted);
}

// static
void InRendererTensorRegistry::Unregister(
    const blink::WebNNTensorToken& token) {
  RegistryStorage& s = Storage();
  base::AutoLock hold(s.lock);
  s.entries.erase(token);
}

// static
scoped_refptr<InRendererTensorHandle> InRendererTensorRegistry::Get(
    const blink::WebNNTensorToken& token) {
  RegistryStorage& s = Storage();
  base::AutoLock hold(s.lock);
  auto it = s.entries.find(token);
  if (it == s.entries.end()) {
    return nullptr;
  }
  return it->second;
}

}  // namespace webnn
