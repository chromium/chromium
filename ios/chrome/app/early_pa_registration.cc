// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/early_zone_registration_apple.h"
#include "partition_alloc/buildflags.h"

__attribute__((constructor(0))) void InitializePartitionAlloc() {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
  // Perform malloc zone registration before loading any dependency as this
  // needs to be called before any thread creation which happens during the
  // initialisation of some of the runtime libraries.
  partition_alloc::EarlyMallocZoneRegistration();
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC)
}
