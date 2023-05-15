// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/skia_utils.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "skia/ext/event_tracer_impl.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "third_party/skia/include/core/SkGraphics.h"

namespace content {
namespace {

// Maximum allocation size allowed for image scaling filters that
// require pre-scaling. Skia will fallback to a filter that doesn't
// require pre-scaling if the default filter would require an
// allocation that exceeds this limit.
const size_t kImageCacheSingleAllocationByteLimit = 64 * 1024 * 1024;

// Decreases the size of the font cache to 1MiB.
BASE_FEATURE(kSmallerFontCache,
             "SmallerFontCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

void InitializeSkia() {
  // Make sure that any switches used here are propagated to the renderer and
  // GPU processes.
  const base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  if (!cmd.HasSwitch(switches::kDisableSkiaRuntimeOpts)) {
    SkGraphics::Init();
  }

  const int kMB = 1024 * 1024;
  size_t font_cache_limit;
#if BUILDFLAG(IS_ANDROID)
  font_cache_limit =
      base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled() ? kMB : 8 * kMB;
  SkGraphics::SetFontCacheLimit(font_cache_limit);
#else
  if (cmd.HasSwitch(switches::kSkiaFontCacheLimitMb)) {
    if (base::StringToSizeT(
            cmd.GetSwitchValueASCII(switches::kSkiaFontCacheLimitMb),
            &font_cache_limit)) {
      SkGraphics::SetFontCacheLimit(font_cache_limit * kMB);
    }
  }

  size_t resource_cache_limit;
  if (cmd.HasSwitch(switches::kSkiaResourceCacheLimitMb)) {
    if (base::StringToSizeT(
            cmd.GetSwitchValueASCII(switches::kSkiaResourceCacheLimitMb),
            &resource_cache_limit)) {
      SkGraphics::SetResourceCacheTotalByteLimit(resource_cache_limit * kMB);
    }
  }
#endif

  if (base::FeatureList::IsEnabled(kSmallerFontCache)) {
    // Could also reduce the maximum number of cached strikes, but the intent
    // being to reduce memory usage, only control cache memory usage.
    SkGraphics::SetFontCacheLimit(kMB);
  }

  InitSkiaEventTracer();
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      skia::SkiaMemoryDumpProvider::GetInstance(), "Skia", nullptr);

  SkGraphics::SetResourceCacheSingleAllocationByteLimit(
      kImageCacheSingleAllocationByteLimit);
}

}  // namespace content
