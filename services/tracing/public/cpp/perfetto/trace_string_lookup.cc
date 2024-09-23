// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/tracing/public/cpp/perfetto/trace_string_lookup.h"

#include "base/strings/pattern.h"

using ::perfetto::protos::pbzero::ChromeThreadDescriptor;

namespace tracing {

struct ThreadType {
  const char* name;
  ChromeThreadDescriptor::ThreadType type;
};

constexpr ThreadType kThreadTypes[] = {
    {"CrBrowserMain", ChromeThreadDescriptor::THREAD_BROWSER_MAIN},
    {"CrRendererMain", ChromeThreadDescriptor::THREAD_RENDERER_MAIN},
    {"CrGpuMain", ChromeThreadDescriptor::THREAD_GPU_MAIN},
    {"CrUtilityMain", ChromeThreadDescriptor::THREAD_UTILITY_MAIN},
    {"CrPPAPIMain", ChromeThreadDescriptor::THREAD_PPAPI_MAIN},
    // Catch-all, should appear after all the other main threads.
    {"Cr*Main", ChromeThreadDescriptor::THREAD_MAIN},

    {"Chrome_ChildIOThread", ChromeThreadDescriptor::THREAD_CHILD_IO},
    {"Chrome_IOThread", ChromeThreadDescriptor::THREAD_BROWSER_IO},
    // Catch-all, should appear after the other I/O threads.
    {"Chrome*IOThread", ChromeThreadDescriptor::THREAD_IO},

    {"ThreadPoolForegroundWorker*",
     ChromeThreadDescriptor::THREAD_POOL_FG_WORKER},
    {"Compositor", ChromeThreadDescriptor::THREAD_COMPOSITOR},
    {"StackSamplingProfiler", ChromeThreadDescriptor::THREAD_SAMPLING_PROFILER},
    {"CompositorTileWorker*", ChromeThreadDescriptor::THREAD_COMPOSITOR_WORKER},
    {"CacheThread_BlockFile", ChromeThreadDescriptor::THREAD_CACHE_BLOCKFILE},
    {"Media", ChromeThreadDescriptor::ChromeThreadDescriptor::THREAD_MEDIA},
    {"NetworkService", ChromeThreadDescriptor::THREAD_NETWORK_SERVICE},
    {"ThreadPoolBackgroundWorker*",
     ChromeThreadDescriptor::THREAD_POOL_BG_WORKER},
    {"ThreadPool*ForegroundBlocking*",
     ChromeThreadDescriptor::THREAD_POOL_FG_BLOCKING},
    {"ThreadPool*BackgroundBlocking*",
     ChromeThreadDescriptor::THREAD_POOL_BG_BLOCKING},
    {"ThreadPoolService*", ChromeThreadDescriptor::THREAD_POOL_SERVICE},
    {"VizCompositor*", ChromeThreadDescriptor::THREAD_VIZ_COMPOSITOR},
    {"ServiceWorker*", ChromeThreadDescriptor::THREAD_SERVICE_WORKER},
    {"MemoryInfra", ChromeThreadDescriptor::THREAD_MEMORY_INFRA},
    {"AudioOutputDevice", ChromeThreadDescriptor::THREAD_AUDIO_OUTPUTDEVICE},
    {"GpuMemoryThread", ChromeThreadDescriptor::THREAD_GPU_MEMORY},
    {"GpuVSyncThread", ChromeThreadDescriptor::THREAD_GPU_VSYNC},
    {"DXVAVideoDecoderThread", ChromeThreadDescriptor::THREAD_DXA_VIDEODECODER},
    {"BrowserWatchdog", ChromeThreadDescriptor::THREAD_BROWSER_WATCHDOG},
    {"WebRTC_Network", ChromeThreadDescriptor::THREAD_WEBRTC_NETWORK},
    {"Window owner thread", ChromeThreadDescriptor::THREAD_WINDOW_OWNER},
    {"WebRTC_Signaling", ChromeThreadDescriptor::THREAD_WEBRTC_SIGNALING},
    {"GpuWatchdog", ChromeThreadDescriptor::THREAD_GPU_WATCHDOG},
    {"swapper", ChromeThreadDescriptor::THREAD_SWAPPER},
    {"Gamepad polling thread", ChromeThreadDescriptor::THREAD_GAMEPAD_POLLING},
    {"AudioInputDevice", ChromeThreadDescriptor::THREAD_AUDIO_INPUTDEVICE},
    {"WebRTC_Worker", ChromeThreadDescriptor::THREAD_WEBRTC_WORKER},
    {"WebCrypto", ChromeThreadDescriptor::THREAD_WEBCRYPTO},
    {"Database thread", ChromeThreadDescriptor::THREAD_DATABASE},
    {"Proxy Resolver", ChromeThreadDescriptor::THREAD_PROXYRESOLVER},
    {"Chrome_DevToolsADBThread", ChromeThreadDescriptor::THREAD_DEVTOOLSADB},
    {"NetworkConfigWatcher",
     ChromeThreadDescriptor::THREAD_NETWORKCONFIGWATCHER},
    {"wasapi_render_thread", ChromeThreadDescriptor::THREAD_WASAPI_RENDER},
    {"LoaderLockSampler", ChromeThreadDescriptor::THREAD_LOADER_LOCK_SAMPLER},
};

ChromeThreadDescriptor::ThreadType GetThreadType(
    const char* const thread_name) {
  for (size_t i = 0; i < std::size(kThreadTypes); ++i) {
    if (base::MatchPattern(thread_name, kThreadTypes[i].name)) {
      return kThreadTypes[i].type;
    }
  }

  return ChromeThreadDescriptor::THREAD_UNSPECIFIED;
}

}  // namespace tracing
