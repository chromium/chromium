// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_MEMORY_MEMORY_INFO_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_MEMORY_MEMORY_INFO_PROVIDER_H_

#include "base/lazy_instance.h"
#include "extensions/browser/api/system_info/system_info_provider.h"
#include "extensions/common/api/system_memory.h"

namespace extensions {

class MemoryInfoProvider : public SystemInfoProvider {
 public:
  MemoryInfoProvider(const MemoryInfoProvider&) = delete;
  MemoryInfoProvider& operator=(const MemoryInfoProvider&) = delete;

  static MemoryInfoProvider* Get();

  const api::system_memory::MemoryInfo& memory_info() const { return info_; }

  static void InitializeForTesting(scoped_refptr<MemoryInfoProvider> provider);

 private:
  friend class MockMemoryInfoProviderImpl;

  MemoryInfoProvider() = default;
  ~MemoryInfoProvider() override = default;

  // Overriden from SystemInfoProvider.
  bool QueryInfo() override;

  // The last information filled up by QueryInfo and is accessed on multiple
  // threads, but the whole class is being guarded by SystemInfoProvider base
  // class.
  //
  // |info_| is accessed on the UI thread while |is_waiting_for_completion_| is
  // false and on the sequenced worker pool while |is_waiting_for_completion_|
  // is true.
  api::system_memory::MemoryInfo info_;

  static base::LazyInstance<scoped_refptr<MemoryInfoProvider>>::DestructorAtExit
      provider_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_MEMORY_MEMORY_INFO_PROVIDER_H_
