// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/skia_utils.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkTraceMemoryDump.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/trace_util.h"

namespace gpu {
namespace raster {
namespace {

// Derives from SkTraceMemoryDump and implements graphics specific memory
// backing functionality.
class SkiaGpuTraceMemoryDump : public SkTraceMemoryDump {
 public:
  // This should never outlive the provided ProcessMemoryDump, as it should
  // always be scoped to a single OnMemoryDump funciton call.
  SkiaGpuTraceMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                         base::Optional<uint64_t> share_group_tracing_guid)
      : pmd_(pmd),
        share_group_tracing_guid_(share_group_tracing_guid),
        tracing_process_id_(base::trace_event::MemoryDumpManager::GetInstance()
                                ->GetTracingProcessId()) {}

  ~SkiaGpuTraceMemoryDump() override = default;

  // Overridden from SkTraceMemoryDump:
  void dumpNumericValue(const char* dump_name,
                        const char* value_name,
                        const char* units,
                        uint64_t value) override {
    auto* dump = GetOrCreateAllocatorDump(dump_name);
    dump->AddScalar(value_name, units, value);
  }

  void setMemoryBacking(const char* dump_name,
                        const char* backing_type,
                        const char* backing_object_id) override {
    // For uniformity, skia provides this value as a string. Convert back to a
    // uint32_t.
    uint32_t gl_id =
        std::strtoul(backing_object_id, nullptr /* str_end */, 10 /* base */);

    // Constants used by SkiaGpuTraceMemoryDump to identify different memory
    // types.
    const char* kGLTextureBackingType = "gl_texture";
    const char* kGLBufferBackingType = "gl_buffer";
    const char* kGLRenderbufferBackingType = "gl_renderbuffer";

    // Populated in if statements below.
    base::trace_event::MemoryAllocatorDumpGuid guid;

    if (share_group_tracing_guid_) {
      // If we have a |share_group_tracing_guid_|, we are in a render process
      // and need to create client texture GUIDs for aliasing with the GPU
      // process.
      if (strcmp(backing_type, kGLTextureBackingType) == 0) {
        guid = gl::GetGLTextureClientGUIDForTracing(*share_group_tracing_guid_,
                                                    gl_id);
      } else if (strcmp(backing_type, kGLBufferBackingType) == 0) {
        guid = gl::GetGLBufferGUIDForTracing(tracing_process_id_, gl_id);
      } else if (strcmp(backing_type, kGLRenderbufferBackingType) == 0) {
        guid = gl::GetGLRenderbufferGUIDForTracing(tracing_process_id_, gl_id);
      }
    } else {
      // If we do not have a |share_group_tracing_guid_|, we are in the GPU
      // process, being used for OOP-R. We need to create Raster dumps for
      // aliasing with the transfer cache. Note that this is currently only
      // needed for textures (not buffers or renderbuffers).
      if (strcmp(backing_type, kGLTextureBackingType) == 0) {
        guid = gl::GetGLTextureRasterGUIDForTracing(gl_id);
      }
    }

    if (!guid.empty()) {
      pmd_->CreateSharedGlobalAllocatorDump(guid);

      auto* dump = GetOrCreateAllocatorDump(dump_name);

      const int kImportance = 2;
      pmd_->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  void setDiscardableMemoryBacking(
      const char* dump_name,
      const SkDiscardableMemory& discardable_memory_object) override {
    // We don't use this class for dumping discardable memory.
    NOTREACHED();
  }

  LevelOfDetail getRequestedDetails() const override {
    // TODO(ssid): Use MemoryDumpArgs to create light dumps when requested
    // (crbug.com/499731).
    return kObjectsBreakdowns_LevelOfDetail;
  }

  bool shouldDumpWrappedObjects() const override {
    // Chrome already dumps objects it imports into Skia. Avoid duplicate dumps
    // by asking Skia not to dump them.
    return false;
  }

 private:
  // Helper to create allocator dumps.
  base::trace_event::MemoryAllocatorDump* GetOrCreateAllocatorDump(
      const char* dump_name) {
    auto* dump = pmd_->GetAllocatorDump(dump_name);
    if (!dump)
      dump = pmd_->CreateAllocatorDump(dump_name);
    return dump;
  }

  base::trace_event::ProcessMemoryDump* pmd_;
  base::Optional<uint64_t> share_group_tracing_guid_;
  uint64_t tracing_process_id_;

  DISALLOW_COPY_AND_ASSIGN(SkiaGpuTraceMemoryDump);
};

}  // namespace

void DetermineGrCacheLimitsFromAvailableMemory(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes) {
  // Default limits.
#if defined(OS_FUCHSIA)
  // Reduce protected budget on fuchsia due to https://fxb/36620.
  constexpr size_t kMaxGaneshResourceCacheBytes = 24 * 1024 * 1024;
#else
  constexpr size_t kMaxGaneshResourceCacheBytes = 96 * 1024 * 1024;
#endif  // defined(OS_FUCHSIA)
  constexpr size_t kMaxDefaultGlyphCacheTextureBytes = 2048 * 1024 * 4;

  *max_resource_cache_bytes = kMaxGaneshResourceCacheBytes;
  *max_glyph_cache_texture_bytes = kMaxDefaultGlyphCacheTextureBytes;

// We can't call AmountOfPhysicalMemory under NACL, so leave the default.
#if !defined(OS_NACL)
  // The limit of the bytes allocated toward GPU resources in the GrContext's
  // GPU cache.
#if defined(OS_FUCHSIA)
  // Reduce protected budget on fuchsia due to https://fxb/36620.
  constexpr size_t kMaxLowEndGaneshResourceCacheBytes = 24 * 1024 * 1024;
#else
  constexpr size_t kMaxLowEndGaneshResourceCacheBytes = 48 * 1024 * 1024;
#endif  // defined(OS_FUCHSIA)
  constexpr size_t kMaxHighEndGaneshResourceCacheBytes = 256 * 1024 * 1024;
  // Limits for glyph cache textures.
  constexpr size_t kMaxLowEndGlyphCacheTextureBytes = 1024 * 512 * 4;
  // High-end / low-end memory cutoffs.
  constexpr int64_t kHighEndMemoryThreshold = (int64_t)4096 * 1024 * 1024;
  constexpr int64_t kLowEndMemoryThreshold = (int64_t)512 * 1024 * 1024;

  int64_t amount_of_physical_memory = base::SysInfo::AmountOfPhysicalMemory();
  if (amount_of_physical_memory <= kLowEndMemoryThreshold) {
    *max_resource_cache_bytes = kMaxLowEndGaneshResourceCacheBytes;
    *max_glyph_cache_texture_bytes = kMaxLowEndGlyphCacheTextureBytes;
  } else if (amount_of_physical_memory >= kHighEndMemoryThreshold) {
    *max_resource_cache_bytes = kMaxHighEndGaneshResourceCacheBytes;
  }
#endif
}

void DefaultGrCacheLimitsForTests(size_t* max_resource_cache_bytes,
                                  size_t* max_glyph_cache_texture_bytes) {
  constexpr size_t kDefaultGlyphCacheTextureBytes = 2048 * 1024 * 4;
  constexpr size_t kDefaultGaneshResourceCacheBytes = 96 * 1024 * 1024;
  *max_resource_cache_bytes = kDefaultGaneshResourceCacheBytes;
  *max_glyph_cache_texture_bytes = kDefaultGlyphCacheTextureBytes;
}

void DumpGrMemoryStatistics(const GrContext* context,
                            base::trace_event::ProcessMemoryDump* pmd,
                            base::Optional<uint64_t> tracing_guid) {
  SkiaGpuTraceMemoryDump trace_memory_dump(pmd, tracing_guid);
  context->dumpMemoryStatistics(&trace_memory_dump);
}

void DumpBackgroundGrMemoryStatistics(
    const GrContext* context,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;

  size_t skia_gr_cache_size;
  context->getResourceCacheUsage(nullptr /* resourceCount */,
                                 &skia_gr_cache_size);
  std::string dump_name =
      base::StringPrintf("skia/gpu_resources/context_0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(context));
  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, skia_gr_cache_size);
}

}  // namespace raster
}  // namespace gpu
