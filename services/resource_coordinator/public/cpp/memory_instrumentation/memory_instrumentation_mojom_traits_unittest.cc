// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation_mojom_traits.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"  // for testing::Contains.
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryAllocatorDumpGuid;
using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpRequestArgs;
using base::trace_event::MemoryDumpType;
using base::trace_event::ProcessMemoryDump;
using testing::ByRef;
using testing::Contains;
using testing::Eq;
using testing::Pointee;

namespace {

using StructTraitsTest = testing::Test;

// Test StructTrait serialization and deserialization for copyable type. |input|
// will be serialized and then deserialized into |output|.
template <class MojomType, class Type>
void SerializeAndDeserialize(const Type& input, Type* output) {
  MojomType::Deserialize(MojomType::Serialize(&input), output);
}

}  // namespace

TEST_F(StructTraitsTest, MemoryDumpRequestArgs) {
  MemoryDumpRequestArgs input{10u, MemoryDumpType::kSummaryOnly,
                              MemoryDumpLevelOfDetail::kDetailed};
  MemoryDumpRequestArgs output;
  SerializeAndDeserialize<mojom::RequestArgs>(input, &output);
  EXPECT_EQ(10u, output.dump_guid);
  EXPECT_EQ(MemoryDumpType::kSummaryOnly, output.dump_type);
  EXPECT_EQ(MemoryDumpLevelOfDetail::kDetailed, output.level_of_detail);
}

TEST_F(StructTraitsTest, MemoryAllocatorDumpEdge) {
  ProcessMemoryDump::MemoryAllocatorDumpEdge input{
      MemoryAllocatorDumpGuid(42), MemoryAllocatorDumpGuid(43), -99, true};
  ProcessMemoryDump::MemoryAllocatorDumpEdge output;
  SerializeAndDeserialize<mojom::RawAllocatorDumpEdge>(input, &output);
  EXPECT_EQ(42u, output.source.ToUint64());
  EXPECT_TRUE(input.source == output.source);
  EXPECT_EQ(43u, output.target.ToUint64());
  EXPECT_TRUE(input.target == output.target);
  EXPECT_EQ(-99, output.importance);
  EXPECT_TRUE(output.overridable);
}

TEST_F(StructTraitsTest, MemoryAllocatorDumpEdgeWithStringIDs) {
  ProcessMemoryDump::MemoryAllocatorDumpEdge input{
      MemoryAllocatorDumpGuid("string1"), MemoryAllocatorDumpGuid("string2"), 0,
      false};
  ProcessMemoryDump::MemoryAllocatorDumpEdge output;
  SerializeAndDeserialize<mojom::RawAllocatorDumpEdge>(input, &output);
  EXPECT_TRUE(input.source == output.source);
  EXPECT_EQ(input.source.ToString(), output.source.ToString());
  EXPECT_TRUE(input.target == output.target);
  EXPECT_EQ(input.target.ToString(), output.target.ToString());
  EXPECT_EQ(0, output.importance);
  EXPECT_FALSE(output.overridable);
}

TEST_F(StructTraitsTest, MemoryAllocatorDumpEntry) {
  MemoryAllocatorDump::Entry input1("name_uint64", "units_uint64", 42);
  MemoryAllocatorDump::Entry output1;
  SerializeAndDeserialize<mojom::RawAllocatorDumpEntry>(input1, &output1);
  EXPECT_EQ("name_uint64", output1.name);
  EXPECT_EQ("units_uint64", output1.units);
  EXPECT_EQ(MemoryAllocatorDump::Entry::kUint64, output1.entry_type);
  EXPECT_EQ(42u, output1.value_uint64);

  input1 = MemoryAllocatorDump::Entry("name_string", "units_string", ".");
  SerializeAndDeserialize<mojom::RawAllocatorDumpEntry>(input1, &output1);
  EXPECT_EQ("name_string", output1.name);
  EXPECT_EQ("units_string", output1.units);
  EXPECT_EQ(MemoryAllocatorDump::Entry::kString, output1.entry_type);
  EXPECT_EQ(".", output1.value_string);

  // Test move operators
  MemoryAllocatorDump::Entry input2("name_moved", "units_moved", "!");
  MemoryAllocatorDump::Entry output2;
  input1 = std::move(input2);
  SerializeAndDeserialize<mojom::RawAllocatorDumpEntry>(input1, &output2);
  output1 = std::move(output2);
  EXPECT_EQ("name_moved", output1.name);
  EXPECT_EQ("units_moved", output1.units);
  EXPECT_EQ(MemoryAllocatorDump::Entry::kString, output1.entry_type);
  EXPECT_EQ("!", output1.value_string);
}

TEST_F(StructTraitsTest, MemoryAllocatorDump) {
  auto input = std::make_unique<MemoryAllocatorDump>(
      "absolute/name", MemoryDumpLevelOfDetail::kDetailed,
      MemoryAllocatorDumpGuid(42));
  std::unique_ptr<MemoryAllocatorDump> output;
  input->AddScalar("size", "bytes", 10);
  input->AddScalar("count", "number", 20);
  input->set_flags(MemoryAllocatorDump::WEAK);
  SerializeAndDeserialize<mojom::RawAllocatorDump>(input, &output);

  EXPECT_EQ(42u, output->guid().ToUint64());
  EXPECT_EQ("absolute/name", output->absolute_name());
  EXPECT_EQ(MemoryAllocatorDump::WEAK, output->flags());
  EXPECT_EQ(10u, output->GetSizeInternal());
  EXPECT_EQ(MemoryDumpLevelOfDetail::kDetailed, output->level_of_detail());
  MemoryAllocatorDump::Entry expected_entry1("size", "bytes", 10);
  EXPECT_THAT(output->entries(), Contains(Eq(ByRef(expected_entry1))));
  MemoryAllocatorDump::Entry expected_entry2("count", "number", 20);
  EXPECT_THAT(output->entries(), Contains(Eq(ByRef(expected_entry2))));
}

TEST_F(StructTraitsTest, ProcessMemoryDump) {
  auto input = std::make_unique<ProcessMemoryDump>(
      MemoryDumpArgs{MemoryDumpLevelOfDetail::kDetailed});
  std::unique_ptr<ProcessMemoryDump> output;
  MemoryAllocatorDump* mad1 = input->CreateAllocatorDump("mad/1");
  MemoryAllocatorDumpGuid mad1_id = mad1->guid();
  mad1->AddScalar("size", "bytes", 10);
  mad1->AddScalar("count", "number", 11);

  MemoryAllocatorDump* mad2 = input->CreateAllocatorDump("mad/2");
  MemoryAllocatorDumpGuid mad2_id = mad2->guid();
  mad2->AddScalar("size", "bytes", 20);

  MemoryAllocatorDump* mad3 = input->CreateAllocatorDump("mad/3");
  mad3->AddScalar("count", "number", 31);
  input->AddOwnershipEdge(mad2_id, mad1_id, -42);

  MemoryAllocatorDump* mad_shg =
      input->CreateSharedGlobalAllocatorDump(MemoryAllocatorDumpGuid(1));
  mad_shg->AddString("shared_name", "url", "!");

  MemoryAllocatorDump* mad_wshg =
      input->CreateWeakSharedGlobalAllocatorDump(MemoryAllocatorDumpGuid(2));
  mad_wshg->AddString("shared_weak_name", "url", ".");
  SerializeAndDeserialize<mojom::RawProcessMemoryDump>(input, &output);

  EXPECT_EQ(MemoryDumpLevelOfDetail::kDetailed,
            output->dump_args().level_of_detail);
  const auto& dumps = output->allocator_dumps();
  {
    auto mad_it = dumps.find("mad/1");
    ASSERT_NE(dumps.end(), mad_it);
    MemoryAllocatorDump::Entry expected_entry1("size", "bytes", 10);
    MemoryAllocatorDump::Entry expected_entry2("count", "number", 11);
    EXPECT_THAT(mad_it->second->entries(),
                Contains(Eq(ByRef(expected_entry1))));
    EXPECT_THAT(mad_it->second->entries(),
                Contains(Eq(ByRef(expected_entry2))));
  }
  {
    auto mad_it = dumps.find("mad/2");
    ASSERT_NE(dumps.end(), mad_it);
    MemoryAllocatorDump::Entry expected_entry("size", "bytes", 20);
    EXPECT_EQ(1u, mad_it->second->entries().size());
    EXPECT_THAT(mad_it->second->entries(), Contains(Eq(ByRef(expected_entry))));
  }
  {
    auto mad_it = dumps.find("mad/3");
    ASSERT_NE(dumps.end(), mad_it);
    MemoryAllocatorDump::Entry expected_entry("count", "number", 31);
    EXPECT_EQ(1u, mad_it->second->entries().size());
    EXPECT_THAT(mad_it->second->entries(), Contains(Eq(ByRef(expected_entry))));
  }
  {
    auto mad_it = dumps.find("global/1");
    ASSERT_NE(dumps.end(), mad_it);
    EXPECT_FALSE(mad_it->second->flags() & MemoryAllocatorDump::WEAK);
    MemoryAllocatorDump::Entry expected_entry("shared_name", "url", "!");
    EXPECT_EQ(1u, mad_it->second->entries().size());
    EXPECT_THAT(mad_it->second->entries(), Contains(Eq(ByRef(expected_entry))));
  }
  {
    auto mad_it = dumps.find("global/2");
    ASSERT_NE(dumps.end(), mad_it);
    EXPECT_TRUE(mad_it->second->flags() & MemoryAllocatorDump::WEAK);
    MemoryAllocatorDump::Entry expected_entry("shared_weak_name", "url", ".");
    EXPECT_EQ(1u, mad_it->second->entries().size());
    EXPECT_THAT(mad_it->second->entries(), Contains(Eq(ByRef(expected_entry))));
  }
  const auto& edges = output->allocator_dumps_edges();
  {
    auto edge_it = edges.find(mad2_id);
    ASSERT_NE(edges.end(), edge_it);
    EXPECT_TRUE(ProcessMemoryDump::MemoryAllocatorDumpEdge(
                    {mad2_id, mad1_id, -42, false}) == edge_it->second);
  }
  ASSERT_EQ(0u, edges.count(MemoryAllocatorDumpGuid(1)));
}

}  // namespace memory_instrumentation
