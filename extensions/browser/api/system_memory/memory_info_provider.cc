// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_memory/memory_info_provider.h"

#include "base/system/sys_info.h"

namespace extensions {

using api::system_memory::MemoryInfo;

// Static member intialization.
base::LazyInstance<scoped_refptr<MemoryInfoProvider>>::DestructorAtExit
    MemoryInfoProvider::provider_ = LAZY_INSTANCE_INITIALIZER;

void MemoryInfoProvider::InitializeForTesting(
    scoped_refptr<MemoryInfoProvider> provider) {
  DCHECK(provider.get() != nullptr);
  provider_.Get() = provider;
}

bool MemoryInfoProvider::QueryInfo() {
  info_.capacity = static_cast<double>(base::SysInfo::AmountOfPhysicalMemory());
  info_.available_capacity =
      static_cast<double>(base::SysInfo::AmountOfAvailablePhysicalMemory());
  return true;
}

// static
MemoryInfoProvider* MemoryInfoProvider::Get() {
  if (provider_.Get().get() == nullptr) {
    provider_.Get() = new MemoryInfoProvider();
  }
  return provider_.Get().get();
}

}  // namespace extensions
