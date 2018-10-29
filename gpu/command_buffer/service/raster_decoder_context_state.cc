// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder_context_state.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/create_gr_gl_interface.h"

namespace gpu {
namespace raster {

RasterDecoderContextState::RasterDecoderContextState(
    scoped_refptr<gl::GLShareGroup> share_group,
    scoped_refptr<gl::GLSurface> surface,
    scoped_refptr<gl::GLContext> context,
    bool use_virtualized_gl_contexts)
    : share_group(std::move(share_group)),
      surface(std::move(surface)),
      context(std::move(context)),
      use_virtualized_gl_contexts(use_virtualized_gl_contexts) {
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "RasterDecoderContextState", base::ThreadTaskRunnerHandle::Get());
  }
}

RasterDecoderContextState::~RasterDecoderContextState() {
  if (gr_context)
    gr_context->abandonContext();
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void RasterDecoderContextState::InitializeGrContext(
    const GpuDriverBugWorkarounds& workarounds,
    GrContextOptions::PersistentCache* cache,
    GpuProcessActivityFlags* activity_flags,
    gl::ProgressReporter* progress_reporter) {
  DCHECK(context->IsCurrent(surface.get()));

  sk_sp<GrGLInterface> interface(gl::init::CreateGrGLInterface(
      *context->GetVersionInfo(), workarounds.use_es2_for_oopr,
      progress_reporter));
  if (!interface) {
    LOG(ERROR) << "OOP raster support disabled: GrGLInterface creation "
                  "failed.";
    return;
  }

  if (activity_flags && cache) {
    // |activity_flags| is safe to capture here since it must outlive the
    // this context state.
    interface->fFunctions.fProgramBinary =
        [activity_flags](GrGLuint program, GrGLenum binaryFormat, void* binary,
                         GrGLsizei length) {
          GpuProcessActivityFlags::ScopedSetFlag scoped_set_flag(
              activity_flags, ActivityFlagsBase::FLAG_LOADING_PROGRAM_BINARY);
          glProgramBinary(program, binaryFormat, binary, length);
        };
  }

  // If you make any changes to the GrContext::Options here that could
  // affect text rendering, make sure to match the capabilities initialized
  // in GetCapabilities and ensuring these are also used by the
  // PaintOpBufferSerializer.
  GrContextOptions options;
  options.fDriverBugWorkarounds =
      GrDriverBugWorkarounds(workarounds.ToIntSet());
  size_t max_resource_cache_bytes = 0u;
  raster::DetermineGrCacheLimitsFromAvailableMemory(
      &max_resource_cache_bytes, &glyph_cache_max_texture_bytes);
  options.fGlyphCacheTextureMaximumBytes = glyph_cache_max_texture_bytes;
  options.fPersistentCache = cache;
  options.fAvoidStencilBuffers = workarounds.avoid_stencil_buffers;
  gr_context = GrContext::MakeGL(std::move(interface), options);
  if (!gr_context) {
    LOG(ERROR) << "OOP raster support disabled: GrContext creation "
                  "failed.";
  } else {
    constexpr int kMaxGaneshResourceCacheCount = 16384;
    gr_context->setResourceCacheLimits(kMaxGaneshResourceCacheCount,
                                       max_resource_cache_bytes);
  }

  transfer_cache = std::make_unique<ServiceTransferCache>();
}

bool RasterDecoderContextState::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (gr_context)
    DumpGrMemoryStatistics(gr_context.get(), pmd, base::nullopt);
  return true;
}

void RasterDecoderContextState::PurgeMemory(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (!gr_context) {
    DCHECK(!transfer_cache);
    return;
  }

  // Ensure the context is current before doing any GPU cleanup.
  context->MakeCurrent(surface.get());

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      // This function is only called with moderate or critical pressure.
      NOTREACHED();
      return;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      // With moderate pressure, clear any unlocked resources.
      gr_context->purgeUnlockedResources(true /* scratchResourcesOnly */);
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // With critical pressure, purge as much as possible.
      gr_context->freeGpuResources();
      break;
  }

  transfer_cache->PurgeMemory(memory_pressure_level);
}

}  // namespace raster
}  // namespace gpu
