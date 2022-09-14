// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_MEMORY_ALLOCATOR_DUMP_CROSS_PROCESS_UID_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_MEMORY_ALLOCATOR_DUMP_CROSS_PROCESS_UID_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "mojo/public/mojom/base/memory_allocator_dump_cross_process_uid.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    StructTraits<mojo_base::mojom::MemoryAllocatorDumpCrossProcessUidDataView,
                 base::trace_event::MemoryAllocatorDumpGuid> {
  static uint64_t value(const base::trace_event::MemoryAllocatorDumpGuid& id) {
    return id.ToUint64();
  }

  static bool Read(
      mojo_base::mojom::MemoryAllocatorDumpCrossProcessUidDataView data,
      base::trace_event::MemoryAllocatorDumpGuid* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_MEMORY_ALLOCATOR_DUMP_CROSS_PROCESS_UID_MOJOM_TRAITS_H_
