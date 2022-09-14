// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/memory_allocator_dump_cross_process_uid_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::MemoryAllocatorDumpCrossProcessUidDataView,
                  base::trace_event::MemoryAllocatorDumpGuid>::
    Read(mojo_base::mojom::MemoryAllocatorDumpCrossProcessUidDataView data,
         base::trace_event::MemoryAllocatorDumpGuid* out) {
  // Receiving a zeroed MemoryAllocatorDumpCrossProcessUid is a bug.
  if (data.value() == 0)
    return false;

  *out = base::trace_event::MemoryAllocatorDumpGuid(data.value());
  return true;
}

}  // namespace mojo