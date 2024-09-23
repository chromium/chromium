// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/partition_alloc_memory_dump_provider.h"

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/malloc_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "partition_alloc/partition_alloc.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

const char kPartitionAllocDumpName[] = "partition_alloc";

PartitionAllocMemoryDumpProvider* PartitionAllocMemoryDumpProvider::Instance() {
  DEFINE_STATIC_LOCAL(PartitionAllocMemoryDumpProvider, instance, ());
  return &instance;
}

bool PartitionAllocMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* memory_dump) {
  using base::trace_event::MemoryDumpLevelOfDetail;

  MemoryDumpLevelOfDetail level_of_detail = args.level_of_detail;
  base::trace_event::MemoryDumpPartitionStatsDumper partition_stats_dumper(
      kPartitionAllocDumpName, memory_dump, level_of_detail);

  base::trace_event::MemoryAllocatorDump* partitions_dump =
      memory_dump->CreateAllocatorDump(
          base::StringPrintf("%s/%s", kPartitionAllocDumpName,
                             base::trace_event::MemoryDumpPartitionStatsDumper::
                                 kPartitionsDumpName));

  // This method calls memoryStats.partitionsDumpBucketStats with memory
  // statistics.
  WTF::Partitions::DumpMemoryStats(
      level_of_detail != MemoryDumpLevelOfDetail::kDetailed,
      &partition_stats_dumper);

  base::trace_event::MemoryAllocatorDump* allocated_objects_dump =
      memory_dump->CreateAllocatorDump(
          WTF::Partitions::kAllocatedObjectPoolName);
  allocated_objects_dump->AddScalar(
      "size", "bytes", partition_stats_dumper.total_active_bytes());
  memory_dump->AddOwnershipEdge(allocated_objects_dump->guid(),
                                partitions_dump->guid());

  return true;
}

PartitionAllocMemoryDumpProvider::PartitionAllocMemoryDumpProvider() = default;
PartitionAllocMemoryDumpProvider::~PartitionAllocMemoryDumpProvider() = default;

}  // namespace blink
