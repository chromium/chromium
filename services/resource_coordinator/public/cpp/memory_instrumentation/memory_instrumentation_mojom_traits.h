// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_MOJOM_TRAITS_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MOJOM)
    EnumTraits<memory_instrumentation::mojom::DumpType,
               base::trace_event::MemoryDumpType> {
  static memory_instrumentation::mojom::DumpType ToMojom(
      base::trace_event::MemoryDumpType type);
  static bool FromMojom(memory_instrumentation::mojom::DumpType input,
                        base::trace_event::MemoryDumpType* out);
};

template <>
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MOJOM)
    EnumTraits<memory_instrumentation::mojom::LevelOfDetail,
               base::trace_event::MemoryDumpLevelOfDetail> {
  static memory_instrumentation::mojom::LevelOfDetail ToMojom(
      base::trace_event::MemoryDumpLevelOfDetail level_of_detail);
  static bool FromMojom(memory_instrumentation::mojom::LevelOfDetail input,
                        base::trace_event::MemoryDumpLevelOfDetail* out);
};

template <>
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MOJOM)
    EnumTraits<memory_instrumentation::mojom::Determinism,
               base::trace_event::MemoryDumpDeterminism> {
  static memory_instrumentation::mojom::Determinism ToMojom(
      base::trace_event::MemoryDumpDeterminism determinism);
  static bool FromMojom(memory_instrumentation::mojom::Determinism input,
                        base::trace_event::MemoryDumpDeterminism* out);
};

template <>
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MOJOM)
    StructTraits<memory_instrumentation::mojom::RequestArgsDataView,
                 base::trace_event::MemoryDumpRequestArgs> {
  static uint64_t dump_guid(
      const base::trace_event::MemoryDumpRequestArgs& args) {
    return args.dump_guid;
  }
  static base::trace_event::MemoryDumpType dump_type(
      const base::trace_event::MemoryDumpRequestArgs& args) {
    return args.dump_type;
  }
  static base::trace_event::MemoryDumpLevelOfDetail level_of_detail(
      const base::trace_event::MemoryDumpRequestArgs& args) {
    return args.level_of_detail;
  }
  static base::trace_event::MemoryDumpDeterminism determinism(
      const base::trace_event::MemoryDumpRequestArgs& args) {
    return args.determinism;
  }
  static bool Read(memory_instrumentation::mojom::RequestArgsDataView input,
                   base::trace_event::MemoryDumpRequestArgs* out);
};

template <>
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MOJOM) StructTraits<
    memory_instrumentation::mojom::RawAllocatorDumpEdgeDataView,
    base::trace_event::ProcessMemoryDump::MemoryAllocatorDumpEdge> {
  static uint64_t source_id(
      const base::trace_event::ProcessMemoryDump::MemoryAllocatorDumpEdge&
          edge) {
    return edge.source.ToUint64();
  }
  static uint64_t target_id(
      const base::trace_event::ProcessMemoryDump::MemoryAllocatorDumpEdge&
          edge) {
    return edge.target.ToUint64();
  }
  static int importance(
      const base::trace_event::ProcessMemoryDump::MemoryAllocatorDumpEdge&
          edge) {
    return edge.importance;
  }
  static bool overridable(
      const base::trace_event::ProcessMemoryDump::MemoryAllocatorDumpEdge&
          edge) {
    return edge.overridable;
  }
  static bool Read(
      memory_instrumentation::mojom::RawAllocatorDumpEdgeDataView input,
      base::trace_event::ProcessMemoryDump::MemoryAllocatorDumpEdge* out);
};

template <>
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MOJOM) UnionTraits<
    memory_instrumentation::mojom::RawAllocatorDumpEntryValueDataView,
    base::trace_event::MemoryAllocatorDump::Entry> {
  static memory_instrumentation::mojom::RawAllocatorDumpEntryValueDataView::Tag
  GetTag(const base::trace_event::MemoryAllocatorDump::Entry& args) {
    switch (args.entry_type) {
      case base::trace_event::MemoryAllocatorDump::Entry::EntryType::kUint64:
        return memory_instrumentation::mojom::
            RawAllocatorDumpEntryValueDataView::Tag::kValueUint64;
      case base::trace_event::MemoryAllocatorDump::Entry::EntryType::kString:
        return memory_instrumentation::mojom::
            RawAllocatorDumpEntryValueDataView::Tag::kValueString;
    }
    NOTREACHED_IN_MIGRATION();
    return memory_instrumentation::mojom::RawAllocatorDumpEntryValueDataView::
        Tag::kValueUint64;
  }

  static uint64_t value_uint64(
      const base::trace_event::MemoryAllocatorDump::Entry& args) {
    return args.value_uint64;
  }

  static const std::string& value_string(
      const base::trace_event::MemoryAllocatorDump::Entry& args) {
    return args.value_string;
  }

  static bool Read(
      memory_instrumentation::mojom::RawAllocatorDumpEntryValueDataView data,
      base::trace_event::MemoryAllocatorDump::Entry* out);
};

template <>
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MOJOM)
    StructTraits<memory_instrumentation::mojom::RawAllocatorDumpEntryDataView,
                 base::trace_event::MemoryAllocatorDump::Entry> {
  static const std::string& name(
      const base::trace_event::MemoryAllocatorDump::Entry& entry) {
    return entry.name;
  }
  static const std::string& units(
      const base::trace_event::MemoryAllocatorDump::Entry& entry) {
    return entry.units;
  }
  static const base::trace_event::MemoryAllocatorDump::Entry& value(
      const base::trace_event::MemoryAllocatorDump::Entry& entry) {
    return entry;
  }
  static bool Read(
      memory_instrumentation::mojom::RawAllocatorDumpEntryDataView input,
      base::trace_event::MemoryAllocatorDump::Entry* out);
};

template <>
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MOJOM)
    StructTraits<memory_instrumentation::mojom::RawAllocatorDumpDataView,
                 std::unique_ptr<base::trace_event::MemoryAllocatorDump>> {
  static uint64_t id(
      const std::unique_ptr<base::trace_event::MemoryAllocatorDump>& mad) {
    return mad->guid().ToUint64();
  }
  static const std::string& absolute_name(
      const std::unique_ptr<base::trace_event::MemoryAllocatorDump>& mad) {
    return mad->absolute_name();
  }
  static bool weak(
      const std::unique_ptr<base::trace_event::MemoryAllocatorDump>& mad) {
    return mad->flags() & base::trace_event::MemoryAllocatorDump::WEAK;
  }
  static base::trace_event::MemoryDumpLevelOfDetail level_of_detail(
      const std::unique_ptr<base::trace_event::MemoryAllocatorDump>& mad) {
    return mad->level_of_detail();
  }
  static const std::vector<base::trace_event::MemoryAllocatorDump::Entry>&
  entries(const std::unique_ptr<base::trace_event::MemoryAllocatorDump>& mad) {
    return *mad->mutable_entries_for_serialization();
  }
  static bool Read(
      memory_instrumentation::mojom::RawAllocatorDumpDataView input,
      std::unique_ptr<base::trace_event::MemoryAllocatorDump>* out);
};

template <>
struct COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MOJOM)
    StructTraits<memory_instrumentation::mojom::RawProcessMemoryDumpDataView,
                 std::unique_ptr<base::trace_event::ProcessMemoryDump>> {
  // TODO(primiano): Remove this wrapping vector to adapt the underlying map<>
  // and use ArrayTraits instead (crbug.com/763441).
  static std::vector<
      base::trace_event::ProcessMemoryDump::MemoryAllocatorDumpEdge>
  allocator_dump_edges(
      const std::unique_ptr<base::trace_event::ProcessMemoryDump>& pmd) {
    return pmd->GetAllEdgesForSerialization();
  }
  static std::vector<std::unique_ptr<base::trace_event::MemoryAllocatorDump>>
  allocator_dumps(
      const std::unique_ptr<base::trace_event::ProcessMemoryDump>& pmd) {
    std::vector<std::unique_ptr<base::trace_event::MemoryAllocatorDump>> dumps;
    dumps.reserve(pmd->mutable_allocator_dumps_for_serialization()->size());
    for (auto& it : *pmd->mutable_allocator_dumps_for_serialization())
      dumps.push_back(std::move(it.second));
    return dumps;
  }
  static base::trace_event::MemoryDumpLevelOfDetail level_of_detail(
      const std::unique_ptr<base::trace_event::ProcessMemoryDump>& pmd) {
    return pmd->dump_args().level_of_detail;
  }
  static base::trace_event::MemoryDumpDeterminism determinism(
      const std::unique_ptr<base::trace_event::ProcessMemoryDump>& pmd) {
    return pmd->dump_args().determinism;
  }

  static void SetToNull(
      std::unique_ptr<base::trace_event::ProcessMemoryDump>* out) {
    out->reset();
  }

  static bool IsNull(
      const std::unique_ptr<base::trace_event::ProcessMemoryDump>& pmd) {
    return !pmd;
  }

  static bool Read(
      memory_instrumentation::mojom::RawProcessMemoryDumpDataView input,
      std::unique_ptr<base::trace_event::ProcessMemoryDump>* out);
};

}  // namespace mojo

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_MOJOM_TRAITS_H_
