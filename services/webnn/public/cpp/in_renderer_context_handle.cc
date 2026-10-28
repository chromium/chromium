// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/in_renderer_context_handle.h"

#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace webnn {

InRendererContextHandle::InRendererContextHandle(
    scoped_refptr<base::SequencedTaskRunner> owning_task_runner)
    : base::RefCountedDeleteOnSequence<InRendererContextHandle>(
          std::move(owning_task_runner)) {}
InRendererContextHandle::~InRendererContextHandle() = default;

namespace {

struct RegistryStorage {
  base::Lock lock;
  base::flat_map<blink::WebNNContextToken,
                 scoped_refptr<InRendererContextHandle>>
      entries GUARDED_BY(lock);
};

RegistryStorage& Storage() {
  static base::NoDestructor<RegistryStorage> instance;
  return *instance;
}

}  // namespace

// static
void InRendererContextRegistry::Register(
    const blink::WebNNContextToken& token,
    scoped_refptr<InRendererContextHandle> handle) {
  CHECK(handle);
  RegistryStorage& s = Storage();
  base::AutoLock hold(s.lock);
  auto [_, inserted] = s.entries.emplace(token, std::move(handle));
  CHECK(inserted);
}

// static
void InRendererContextRegistry::Unregister(
    const blink::WebNNContextToken& token) {
  RegistryStorage& s = Storage();
  base::AutoLock hold(s.lock);
  s.entries.erase(token);
}

// static
scoped_refptr<InRendererContextHandle> InRendererContextRegistry::Get(
    const blink::WebNNContextToken& token) {
  RegistryStorage& s = Storage();
  base::AutoLock hold(s.lock);
  auto it = s.entries.find(token);
  if (it == s.entries.end()) {
    return nullptr;
  }
  return it->second;
}

}  // namespace webnn
