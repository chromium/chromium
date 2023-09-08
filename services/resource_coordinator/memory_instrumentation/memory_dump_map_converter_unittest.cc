// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/memory_dump_map_converter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryAllocatorDumpGuid;
using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::ProcessMemoryDump;

TEST(MemoryDumpMapConverter, Convert) {
  MemoryDumpMapConverter::MemoryDumpMap process_dumps;

  MemoryDumpArgs dump_args1 = {MemoryDumpLevelOfDetail::kDetailed};
  ProcessMemoryDump pmd1(dump_args1);

  auto* source1 = pmd1.CreateAllocatorDump("test1/test2/test3");
  source1->AddScalar(MemoryAllocatorDump::kNameSize,
                     MemoryAllocatorDump::kUnitsBytes, 10);

  auto* target1 = pmd1.CreateAllocatorDump("target1");
  pmd1.AddOwnershipEdge(source1->guid(), target1->guid(), 10);

  pmd1.CreateWeakSharedGlobalAllocatorDump(MemoryAllocatorDumpGuid(1));

  process_dumps.emplace(1, &pmd1);

  MemoryDumpArgs dump_args2 = {MemoryDumpLevelOfDetail::kLight};
  ProcessMemoryDump pmd2(dump_args2);

  auto* source2 = pmd2.CreateAllocatorDump("test1/test4/test5");
  source2->AddScalar(MemoryAllocatorDump::kNameSize,
                     MemoryAllocatorDump::kUnitsObjects, 1);

  process_dumps.emplace(2, &pmd2);

  MemoryDumpMapConverter converter;
  auto output_dump_map = converter.Convert(process_dumps);

  ASSERT_EQ(output_dump_map.size(), 2uL);
  ASSERT_NE(output_dump_map.find(1), output_dump_map.end());
  ASSERT_EQ(output_dump_map[1]->allocator_nodes().size(), 3uL);
  ASSERT_NE(output_dump_map[1]->allocator_nodes().find("test1/test2/test3"),
            output_dump_map[1]->allocator_nodes().end());
  {
    const auto& dump =
        *output_dump_map[1]->allocator_nodes().at("test1/test2/test3");

    ASSERT_EQ(dump.entries().size(), 1uL);
    const auto& entry = dump.entries()[0];
    ASSERT_EQ(entry.entry_type, perfetto::trace_processor::RawMemoryGraphNode::
                                    MemoryNodeEntry::kUint64);
    ASSERT_EQ(entry.units, "bytes");
    ASSERT_EQ(entry.value_uint64, 10uL);
  }

  ASSERT_NE(output_dump_map[1]->allocator_nodes().find("target1"),
            output_dump_map[1]->allocator_nodes().end());
  ASSERT_NE(output_dump_map[1]->allocator_nodes().find("global/1"),
            output_dump_map[1]->allocator_nodes().end());

  ASSERT_NE(output_dump_map.find(2), output_dump_map.end());
  ASSERT_EQ(output_dump_map[2]->allocator_nodes().size(), 1uL);
  ASSERT_NE(output_dump_map[2]->allocator_nodes().find("test1/test4/test5"),
            output_dump_map[2]->allocator_nodes().end());

  {
    const auto& dump =
        *output_dump_map[2]->allocator_nodes().at("test1/test4/test5");

    ASSERT_EQ(dump.entries().size(), 1uL);
    const auto& entry = dump.entries()[0];
    ASSERT_EQ(entry.entry_type, perfetto::trace_processor::RawMemoryGraphNode::
                                    MemoryNodeEntry::kUint64);
    ASSERT_EQ(entry.units, "objects");
    ASSERT_EQ(entry.value_uint64, 1uL);
  }
}

}  // namespace memory_instrumentation
