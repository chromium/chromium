// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/partition_alloc_support.h"

#include <string>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/extended_api.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/feature_list.h"
#include "base/partition_alloc_buildflags.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

#if defined(OS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace content {
namespace internal {

namespace {

void EnablePCScanForMallocPartitionsIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && ALLOW_PCSCAN
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(base::features::kPartitionAllocPCScan)) {
    base::allocator::EnablePCScan();
  }
#endif
}

void EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && ALLOW_PCSCAN
  DCHECK(base::FeatureList::GetInstance());
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocPCScanBrowserOnly)) {
    base::allocator::EnablePCScan();
  }
#endif
}

// This function should be executed as early as possible once we can get the
// command line arguments and determine whether the process needs BRP support.
// Until that moment, all heap allocations end up in a slower temporary
// partition with no thread cache and cause heap fragmentation.
//
// Furthermore, since the function has to allocate a new partition, it must
// only run once.
void ConfigurePartitionRefCountSupportIfNeeded(bool enable_ref_count) {
// Note that ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL implies that
// USE_BACKUP_REF_PTR is true.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    BUILDFLAG(ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL)
  base::allocator::ConfigurePartitionRefCountSupport(enable_ref_count);
#endif
}

void ReconfigurePartitionForKnownProcess(const std::string& process_type) {
  DCHECK_NE(process_type, switches::kZygoteProcess);

  ConfigurePartitionRefCountSupportIfNeeded(process_type !=
                                            switches::kRendererProcess);
}

}  // namespace

void ReconfigurePartitionAllocEarlyish(const std::string& process_type) {
  if (process_type != switches::kZygoteProcess) {
    ReconfigurePartitionForKnownProcess(process_type);
  }

  // These initializations are only relevant for PartitionAlloc-Everywhere
  // builds.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  base::allocator::EnablePartitionAllocMemoryReclaimer();

#if defined(OS_ANDROID)
  // The thread cache consumes more memory, especially as long as periodic purge
  // above is disabled. Don't use one on low-memory devices.
  if (base::SysInfo::IsLowEndDevice()) {
    base::DisablePartitionAllocThreadCacheForProcess();
  }
#endif  // defined(OS_ANDROID)

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

void ReconfigurePartitionAllocAfterZygoteFork(const std::string& process_type) {
  ReconfigurePartitionForKnownProcess(process_type);
}

void ReconfigurePartitionAllocAfterFeatureListInit(
    const std::string& process_type) {
  DCHECK_NE(process_type, switches::kZygoteProcess);
  // TODO(bartekn): Switch to DCHECK once confirmed there are no issues.
  CHECK(base::FeatureList::GetInstance());

  EnablePCScanForMallocPartitionsIfNeeded();
  // No specified process type means this is the Browser process.
  if (process_type.empty()) {
    EnablePCScanForMallocPartitionsInBrowserProcessIfNeeded();
  }
}

}  // namespace internal
}  // namespace content