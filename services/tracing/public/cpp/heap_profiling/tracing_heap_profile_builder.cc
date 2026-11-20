// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/heap_profiling/tracing_heap_profile_builder.h"

#include <algorithm>
#include <array>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/hash/hash.h"
#include "base/process/process.h"
#include "base/profiler/module_cache.h"
#include "base/threading/platform_thread.h"
#include "services/tracing/public/cpp/heap_profiling/heap_profiling_data_source.h"
#include "services/tracing/public/cpp/perfetto/perfetto_data_source_names.h"
#include "third_party/perfetto/include/perfetto/protozero/proto_utils.h"
#include "third_party/perfetto/include/perfetto/tracing/platform.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/stack_sample.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet_defaults.pbzero.h"

namespace tracing {

namespace {

// Estimated number of live objects represented by `sample`. Matches
// heap_profiling::MergeSamples(), which computes `inuse_objects` for the
// heap profiling service's reports.
uint64_t EstimateObjectCount(const base::SamplingHeapProfiler::Sample& sample) {
  // PoissonAllocationSampler only samples an allocation once the accumulated
  // bytes, increased by the allocation's size, cross the sampling threshold, so
  // sampled allocations always have a non-zero size.
  DCHECK_GT(sample.size, 0u);
  return std::max<uint64_t>((sample.total + sample.size / 2) / sample.size, 1);
}

}  // namespace

TracingHeapProfileBuilder::TracingHeapProfileBuilder(
    HeapProfilingIncrementalState* incr_state)
    : incr_state_(incr_state) {}

TracingHeapProfileBuilder::~TracingHeapProfileBuilder() = default;

// static
void TracingHeapProfileBuilder::WriteDefaults(
    perfetto::protos::pbzero::TracePacket* trace_packet) {
  trace_packet->set_sequence_flags(
      perfetto::protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);

  auto* defaults = trace_packet->set_trace_packet_defaults();
  auto* stack_sample_defaults = defaults->set_stack_sample_defaults();
  stack_sample_defaults->set_source(kNativeHeapProfilerSourceName);

  // Follower counters shared by all samples on this sequence, positional with
  // StackSample.follower_weights written by WriteSample(). The primary counter
  // (per allocator) carries the Poisson-scaled live bytes.
  auto* object_count = stack_sample_defaults->add_follower_descriptors();
  object_count->set_name(kObjectCountCounterName);
  object_count->set_unit(
      perfetto::protos::pbzero::StackSample::Unit::UNIT_COUNT);
  object_count->set_description(
      "Estimated number of live objects attributed to the sample: "
      "max(1, round(total / size)).");

  auto* object_size = stack_sample_defaults->add_follower_descriptors();
  object_size->set_name(kObjectSizeCounterName);
  object_size->set_unit(
      perfetto::protos::pbzero::StackSample::Unit::UNIT_BYTES);
  object_size->set_description(
      "Size of the sampled allocation, not scaled by the sampling probability. "
      "Summed across samples, this is the unscaled sampled bytes.");
}

void TracingHeapProfileBuilder::WriteSample(
    perfetto::protos::pbzero::TracePacket* trace_packet,
    const base::SamplingHeapProfiler::Sample& sample,
    base::ModuleCache& module_cache) {
  base::AutoReset<perfetto::protos::pbzero::TracePacket*> reset_trace_packet(
      &trace_packet_, trace_packet);
  base::AutoReset<perfetto::protos::pbzero::InternedData*> reset_interned_data(
      &interned_data_, nullptr);

  if (incr_state_ && incr_state_->was_cleared) {
    WriteDefaults(trace_packet);
    incr_state_->was_cleared = false;
  } else {
    trace_packet->set_sequence_flags(
        perfetto::protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
  }

  // Get IIDs first, which might write to interned_data_.
  uint32_t counter_iid = GetOrCreateCounterDescriptor(sample.allocator);
  uint32_t callstack_iid = GetOrCreateCallstack(sample.stack, module_cache);

  // Write to stack_sample last.
  auto* stack_sample = trace_packet_->set_stack_sample();
  stack_sample->set_primary_descriptor_iid(counter_iid);
  stack_sample->set_primary_weight(sample.total);
  stack_sample->set_callstack_iid(callstack_iid);

  // Positional with the StackSampleDefaults.follower_descriptors written by
  // WriteDefaults(). The packed varints are encoded directly, as the generated
  // set_follower_weights() does, because protozero::PackedVarInt isn't exported
  // from the Perfetto component (and reserves an 8 KiB stack buffer).
  constexpr size_t kMaxVarIntSize = 10;
  std::array<uint8_t, 2 * kMaxVarIntSize> follower_weights;
  uint8_t* follower_weights_end = protozero::proto_utils::WriteVarInt(
      EstimateObjectCount(sample), follower_weights.data());
  follower_weights_end = protozero::proto_utils::WriteVarInt(
      static_cast<uint64_t>(sample.size), follower_weights_end);
  stack_sample->AppendBytes(
      perfetto::protos::pbzero::StackSample::kFollowerWeightsFieldNumber,
      follower_weights.data(),
      static_cast<size_t>(follower_weights_end - follower_weights.data()));

  auto* task_context = stack_sample->set_task_context();
  task_context->set_pid(perfetto::Platform::GetCurrentProcessId());
  if (sample.tid != base::kInvalidThreadId) {
    task_context->set_tid(sample.tid.raw());
  }
}

uint32_t TracingHeapProfileBuilder::GetOrCreateCounterDescriptor(
    base::allocator::dispatcher::AllocationSubsystem allocator) {
  uint32_t allocator_val = static_cast<uint32_t>(allocator);
  auto entry = incr_state_->interned_counters_.LookupOrAdd(allocator_val);
  if (!entry.was_emitted) {
    auto* intern_data = GetOrCreateInternedData();
    auto* desc = intern_data->add_stack_sample_counter_descriptors();
    desc->set_iid(entry.id);
    desc->set_name(AllocatorName(allocator));
    desc->set_unit(perfetto::protos::pbzero::StackSample::Unit::UNIT_BYTES);
  }
  return entry.id;
}

uint32_t TracingHeapProfileBuilder::GetOrCreateCallstack(
    const std::vector<const void*>& stack,
    base::ModuleCache& module_cache) {
  size_t ip_hash = 0;
  for (const void* pc : stack) {
    ip_hash = base::HashInts(ip_hash, reinterpret_cast<uintptr_t>(pc));
  }

  auto callstack_entry = incr_state_->interned_callstacks_.LookupOrAdd(ip_hash);
  if (callstack_entry.was_emitted) {
    return callstack_entry.id;
  }

  std::vector<uint32_t> frame_iids;
  for (const void* pc : stack) {
    uint32_t frame_iid = GetOrCreateFrame(pc, module_cache);
    frame_iids.push_back(frame_iid);
  }

  auto* intern_data = GetOrCreateInternedData();
  auto* callstack = intern_data->add_callstacks();
  callstack->set_iid(callstack_entry.id);
  for (size_t i = frame_iids.size(); i > 0; --i) {
    callstack->add_frame_ids(frame_iids[i - 1]);
  }

  return callstack_entry.id;
}

uint32_t TracingHeapProfileBuilder::GetOrCreateFrame(
    const void* pc,
    base::ModuleCache& module_cache) {
  uintptr_t ip = reinterpret_cast<uintptr_t>(pc);

  const base::ModuleCache::Module* module =
      module_cache.GetModuleForAddress(ip);
  std::string module_id;
  uintptr_t rel_pc = ip;
  uintptr_t module_base = 0;

  if (module) {
    module_id = module->GetId();
    module_base = module->GetBaseAddress();
    rel_pc = ip - module_base;
  }

  auto frame_key = std::make_pair(rel_pc, module_id);
  auto frame_entry = incr_state_->interned_frames_.LookupOrAdd(frame_key);
  if (frame_entry.was_emitted) {
    return frame_entry.id;
  }

  uint32_t mapping_iid = 0;
  if (module) {
    mapping_iid = GetOrCreateMapping(module);
  }

  auto* intern_data = GetOrCreateInternedData();
  auto* frame = intern_data->add_frames();
  frame->set_iid(frame_entry.id);
  frame->set_rel_pc(rel_pc);
  if (mapping_iid > 0) {
    frame->set_mapping_id(mapping_iid);
  }

  return frame_entry.id;
}

uint32_t TracingHeapProfileBuilder::GetOrCreateMapping(
    const base::ModuleCache::Module* module) {
  uintptr_t base_address = module->GetBaseAddress();

  auto mapping_entry = incr_state_->interned_modules_.LookupOrAdd(base_address);
  if (mapping_entry.was_emitted) {
    return mapping_entry.id;
  }

  auto* intern_data = GetOrCreateInternedData();

  std::string module_id =
      base::TransformModuleIDToSymbolServerFormat(module->GetId());

  auto build_id_entry =
      incr_state_->interned_module_ids_.LookupOrAdd(module_id);
  if (!build_id_entry.was_emitted) {
    auto* build_id = intern_data->add_build_ids();
    build_id->set_iid(build_id_entry.id);
    build_id->set_str(module_id);
  }

  std::optional<uint32_t> path_iid;
  std::string path = module->GetDebugBasename().MaybeAsASCII();
  if (!path.empty()) {
    auto path_entry = incr_state_->interned_module_paths_.LookupOrAdd(path);
    if (!path_entry.was_emitted) {
      auto* mapping_path = intern_data->add_mapping_paths();
      mapping_path->set_iid(path_entry.id);
      mapping_path->set_str(path);
    }
    path_iid = path_entry.id;
  }

  auto* mapping = intern_data->add_mappings();
  mapping->set_iid(mapping_entry.id);
  mapping->set_build_id(build_id_entry.id);
  if (path_iid) {
    mapping->add_path_string_ids(*path_iid);
  }

  return mapping_entry.id;
}

perfetto::protos::pbzero::InternedData*
TracingHeapProfileBuilder::GetOrCreateInternedData() {
  if (!interned_data_) {
    interned_data_ = trace_packet_->set_interned_data();
  }
  return interned_data_;
}

const char* TracingHeapProfileBuilder::AllocatorName(
    base::allocator::dispatcher::AllocationSubsystem allocator) {
  switch (allocator) {
    case base::allocator::dispatcher::AllocationSubsystem::kAllocatorShim:
      return "malloc";
    case base::allocator::dispatcher::AllocationSubsystem::kPartitionAllocator:
      return "partition_alloc";
    case base::allocator::dispatcher::AllocationSubsystem::kManualForTesting:
      return "manual";
    case base::allocator::dispatcher::AllocationSubsystem::kCppGC:
      return "cppgc";
  }
  NOTREACHED();
}

}  // namespace tracing
