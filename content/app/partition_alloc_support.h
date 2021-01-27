// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_PARTITION_ALLOC_SUPPORT_H_
#define CONTENT_APP_PARTITION_ALLOC_SUPPORT_H_

#include <string>

namespace content {
namespace internal {

// ReconfigurePartitionAlloc* functions re-configure PartitionAlloc. It is
// impossible to configure PartitionAlloc before/at its initialization using
// information not known at compile-time (e.g. process type, Finch), because by
// the time this information is available memory allocations would have surely
// happened, that requiring a functioning allocator.
//
// *Earlyish() is called as early as it is reasonably possible.
// *AfterZygoteFork() is its complement to finish configuring process-specific
// stuff that had to be postponed due to *Earlyish() being called with
// |process_type==kZygoteProcess|.
// *AfterFeatureListInit() is called in addition to the above, once
// FeatureList has been initialized and ready to use. It is guaranteed to be
// called on non-zygote processes or after the zygote has been forked.
void ReconfigurePartitionAllocEarlyish(const std::string& process_type);
void ReconfigurePartitionAllocAfterZygoteFork(const std::string& process_type);
void ReconfigurePartitionAllocAfterFeatureListInit(
    const std::string& process_type);

}  // namespace internal
}  // namespace content

#endif  // CONTENT_APP_PARTITION_ALLOC_SUPPORT_H_