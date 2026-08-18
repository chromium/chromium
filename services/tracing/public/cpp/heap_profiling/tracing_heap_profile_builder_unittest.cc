// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/heap_profiling/tracing_heap_profile_builder.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/profiler/module_cache.h"
#include "services/tracing/public/cpp/heap_profiling/heap_profiling_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_common.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/stack_sample.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

namespace tracing {
namespace {

class TestModule : public base::ModuleCache::Module {
 public:
  TestModule(uintptr_t base_address,
             size_t size,
             const std::string& id,
             const base::FilePath& basename)
      : base_address_(base_address),
        size_(size),
        id_(id),
        basename_(basename) {}

  uintptr_t GetBaseAddress() const override { return base_address_; }
  std::string GetId() const override { return id_; }
  base::FilePath GetDebugBasename() const override { return basename_; }
  size_t GetSize() const override { return size_; }
  bool IsNative() const override { return true; }

 private:
  uintptr_t base_address_;
  size_t size_;
  std::string id_;
  base::FilePath basename_;
};

class TracingHeapProfileBuilderTest : public testing::Test {
 protected:
  HeapProfilingIncrementalState incr_state_;
  base::ModuleCache module_cache_;
};

TEST_F(TracingHeapProfileBuilderTest, WriteDefaults) {
  protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> packet;
  TracingHeapProfileBuilder::WriteDefaults(packet.get());

  std::vector<uint8_t> serialized = packet.SerializeAsArray();
  perfetto::protos::TracePacket parsed;
  ASSERT_TRUE(parsed.ParseFromArray(serialized.data(), serialized.size()));

  EXPECT_EQ(parsed.sequence_flags(),
            static_cast<uint32_t>(
                perfetto::protos::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED));
  ASSERT_TRUE(parsed.has_trace_packet_defaults());
  const auto& defaults = parsed.trace_packet_defaults();
  ASSERT_TRUE(defaults.has_stack_sample_defaults());
  EXPECT_EQ(defaults.stack_sample_defaults().source(),
            "org.chromium.native_heap_profiler");
}

TEST_F(TracingHeapProfileBuilderTest, WriteSampleFirstTime) {
  // Setup module.
  auto module_ptr = std::make_unique<TestModule>(
      0x1000, 0x1000, "module_id_1234",
      base::FilePath(FILE_PATH_LITERAL("my_module")));
  module_cache_.AddCustomNativeModule(std::move(module_ptr));

  // Setup sample.
  base::SamplingHeapProfiler::Sample sample(100, 1000);
  sample.allocator =
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator;
  sample.stack = {reinterpret_cast<const void*>(0x1100)};  // inside module
  sample.tid = base::PlatformThreadId::ForTest(123);

  protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> packet;
  TracingHeapProfileBuilder builder(&incr_state_);
  builder.WriteSample(packet.get(), sample, module_cache_);

  std::vector<uint8_t> serialized = packet.SerializeAsArray();
  perfetto::protos::TracePacket parsed;
  ASSERT_TRUE(parsed.ParseFromArray(serialized.data(), serialized.size()));

  // Verify stack_sample.
  ASSERT_TRUE(parsed.has_stack_sample());
  const auto& stack_sample = parsed.stack_sample();
  EXPECT_EQ(stack_sample.primary_weight(), 1000u);
  EXPECT_GT(stack_sample.primary_descriptor_iid(), 0u);
  EXPECT_GT(stack_sample.callstack_iid(), 0u);
  ASSERT_TRUE(stack_sample.has_task_context());
  EXPECT_EQ(stack_sample.task_context().tid(), 123u);

  // Verify interned_data.
  ASSERT_TRUE(parsed.has_interned_data());
  const auto& interned = parsed.interned_data();

  // 1. Counter Descriptor
  ASSERT_EQ(interned.stack_sample_counter_descriptors_size(), 1);
  const auto& desc = interned.stack_sample_counter_descriptors(0);
  EXPECT_EQ(desc.iid(), stack_sample.primary_descriptor_iid());
  EXPECT_EQ(desc.name(), "partition_alloc");

  // 2. Callstack
  ASSERT_EQ(interned.callstacks_size(), 1);
  const auto& callstack = interned.callstacks(0);
  EXPECT_EQ(callstack.iid(), stack_sample.callstack_iid());
  ASSERT_EQ(callstack.frame_ids_size(), 1);
  uint32_t frame_iid = callstack.frame_ids(0);

  // 3. Frame
  ASSERT_EQ(interned.frames_size(), 1);
  const auto& frame = interned.frames(0);
  EXPECT_EQ(frame.iid(), frame_iid);
  EXPECT_EQ(frame.rel_pc(), 0x100u);  // 0x1100 - 0x1000
  EXPECT_GT(frame.mapping_id(), 0u);

  // 4. Mapping
  ASSERT_EQ(interned.mappings_size(), 1);
  const auto& mapping = interned.mappings(0);
  EXPECT_EQ(mapping.iid(), frame.mapping_id());
  EXPECT_GT(mapping.build_id(), 0u);
  ASSERT_EQ(mapping.path_string_ids_size(), 1);
  uint32_t path_iid = mapping.path_string_ids(0);

  // 5. Build ID
  ASSERT_EQ(interned.build_ids_size(), 1);
  const auto& build_id = interned.build_ids(0);
  EXPECT_EQ(build_id.iid(), mapping.build_id());
  EXPECT_FALSE(build_id.str().empty());

  // 6. Mapping Path
  ASSERT_EQ(interned.mapping_paths_size(), 1);
  const auto& path = interned.mapping_paths(0);
  EXPECT_EQ(path.iid(), path_iid);
  EXPECT_EQ(path.str(), "my_module");
}

TEST_F(TracingHeapProfileBuilderTest, WriteSampleInterning) {
  auto module_ptr = std::make_unique<TestModule>(
      0x1000, 0x1000, "module_id_1234",
      base::FilePath(FILE_PATH_LITERAL("my_module")));
  module_cache_.AddCustomNativeModule(std::move(module_ptr));

  base::SamplingHeapProfiler::Sample sample1(100, 1000);
  sample1.allocator =
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator;
  sample1.stack = {reinterpret_cast<const void*>(0x1100)};

  base::SamplingHeapProfiler::Sample sample2(200, 2000);
  sample2.allocator =
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator;
  sample2.stack = {reinterpret_cast<const void*>(0x1100)};  // Same stack

  TracingHeapProfileBuilder builder(&incr_state_);

  // First sample.
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> packet;
    builder.WriteSample(packet.get(), sample1, module_cache_);
    std::vector<uint8_t> serialized = packet.SerializeAsArray();
    perfetto::protos::TracePacket parsed;
    ASSERT_TRUE(parsed.ParseFromArray(serialized.data(), serialized.size()));

    // Should have interned data.
    EXPECT_TRUE(parsed.has_interned_data());
    EXPECT_GT(parsed.interned_data().callstacks_size(), 0);
  }

  // Second sample (same stack).
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> packet;
    builder.WriteSample(packet.get(), sample2, module_cache_);
    std::vector<uint8_t> serialized = packet.SerializeAsArray();
    perfetto::protos::TracePacket parsed;
    ASSERT_TRUE(parsed.ParseFromArray(serialized.data(), serialized.size()));

    // Should NOT have interned data (reused from state).
    if (parsed.has_interned_data()) {
      EXPECT_EQ(parsed.interned_data().callstacks_size(), 0);
      EXPECT_EQ(parsed.interned_data().frames_size(), 0);
      EXPECT_EQ(parsed.interned_data().mappings_size(), 0);
    }
  }
}

TEST_F(TracingHeapProfileBuilderTest, WriteSamplePartialInterning) {
  auto module_ptr = std::make_unique<TestModule>(
      0x1000, 0x1000, "module_id_1234",
      base::FilePath(FILE_PATH_LITERAL("my_module")));
  module_cache_.AddCustomNativeModule(std::move(module_ptr));

  base::SamplingHeapProfiler::Sample sample1(100, 1000);
  sample1.allocator =
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator;
  sample1.stack = {reinterpret_cast<const void*>(0x1100)};

  base::SamplingHeapProfiler::Sample sample2(200, 2000);
  sample2.allocator =
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator;
  sample2.stack = {reinterpret_cast<const void*>(0x1200)};

  TracingHeapProfileBuilder builder(&incr_state_);

  // First sample.
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> packet;
    builder.WriteSample(packet.get(), sample1, module_cache_);
  }

  // Second sample (different stack, same module).
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> packet;
    builder.WriteSample(packet.get(), sample2, module_cache_);
    std::vector<uint8_t> serialized = packet.SerializeAsArray();
    perfetto::protos::TracePacket parsed;
    ASSERT_TRUE(parsed.ParseFromArray(serialized.data(), serialized.size()));

    ASSERT_TRUE(parsed.has_interned_data());
    const auto& interned = parsed.interned_data();

    // Should have new callstack and frame.
    EXPECT_EQ(interned.callstacks_size(), 1);
    EXPECT_EQ(interned.frames_size(), 1);

    // Should NOT have new mapping (reused).
    EXPECT_EQ(interned.mappings_size(), 0);
  }
}

TEST_F(TracingHeapProfileBuilderTest, WriteSampleEmptyModulePath) {
  auto module_ptr = std::make_unique<TestModule>(
      0x1000, 0x1000, "module_id_1234", base::FilePath());
  module_cache_.AddCustomNativeModule(std::move(module_ptr));

  base::SamplingHeapProfiler::Sample sample(100, 1000);
  sample.allocator =
      base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator;
  sample.stack = {reinterpret_cast<const void*>(0x1100)};

  protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> packet;
  TracingHeapProfileBuilder builder(&incr_state_);
  builder.WriteSample(packet.get(), sample, module_cache_);

  std::vector<uint8_t> serialized = packet.SerializeAsArray();
  perfetto::protos::TracePacket parsed;
  ASSERT_TRUE(parsed.ParseFromArray(serialized.data(), serialized.size()));

  ASSERT_TRUE(parsed.has_interned_data());
  const auto& interned = parsed.interned_data();
  ASSERT_EQ(interned.mappings_size(), 1);
  EXPECT_EQ(interned.mappings(0).path_string_ids_size(), 0);
  EXPECT_EQ(interned.mapping_paths_size(), 0);
}

}  // namespace
}  // namespace tracing
