// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "services/tracing/public/cpp/perfetto/trace_string_lookup.h"

#include <array>

#include "base/strings/pattern.h"

namespace pbzero_enums = ::perfetto::protos::chrome_enums::pbzero;

namespace tracing {

struct ThreadType {
  const char* name;
  pbzero_enums::ThreadType type;
};

constexpr auto kThreadTypes = std::to_array<ThreadType>({
    {"CrBrowserMain", pbzero_enums::THREAD_BROWSER_MAIN},
    {"CrRendererMain", pbzero_enums::THREAD_RENDERER_MAIN},
    {"CrGpuMain", pbzero_enums::THREAD_GPU_MAIN},
    {"CrUtilityMain", pbzero_enums::THREAD_UTILITY_MAIN},
    {"CrPPAPIMain", pbzero_enums::THREAD_PPAPI_MAIN},
    // Catch-all, should appear after all the other main threads.
    {"Cr*Main", pbzero_enums::THREAD_MAIN},

    {"Chrome_ChildIOThread", pbzero_enums::THREAD_CHILD_IO},
    {"Chrome_IOThread", pbzero_enums::THREAD_BROWSER_IO},
    // Catch-all, should appear after the other I/O threads.
    {"Chrome*IOThread", pbzero_enums::THREAD_IO},

    {"ThreadPoolForegroundWorker*", pbzero_enums::THREAD_POOL_FG_WORKER},
    {"Compositor", pbzero_enums::THREAD_COMPOSITOR},
    {"StackSamplingProfiler", pbzero_enums::THREAD_SAMPLING_PROFILER},
    {"CompositorTileWorker*", pbzero_enums::THREAD_COMPOSITOR_WORKER},
    {"CacheThread_BlockFile", pbzero_enums::THREAD_CACHE_BLOCKFILE},
    {"Media", pbzero_enums::THREAD_MEDIA},
    {"NetworkService", pbzero_enums::THREAD_NETWORK_SERVICE},
    {"ThreadPoolBackgroundWorker*", pbzero_enums::THREAD_POOL_BG_WORKER},
    {"ThreadPool*ForegroundBlocking*", pbzero_enums::THREAD_POOL_FG_BLOCKING},
    {"ThreadPool*BackgroundBlocking*", pbzero_enums::THREAD_POOL_BG_BLOCKING},
    {"ThreadPoolService*", pbzero_enums::THREAD_POOL_SERVICE},
    {"VizCompositor*", pbzero_enums::THREAD_VIZ_COMPOSITOR},
    {"ServiceWorker*", pbzero_enums::THREAD_SERVICE_WORKER},
    {"MemoryInfra", pbzero_enums::THREAD_MEMORY_INFRA},
    {"AudioOutputDevice", pbzero_enums::THREAD_AUDIO_OUTPUTDEVICE},
    {"GpuMemoryThread", pbzero_enums::THREAD_GPU_MEMORY},
    {"GpuVSyncThread", pbzero_enums::THREAD_GPU_VSYNC},
    {"DXVAVideoDecoderThread", pbzero_enums::THREAD_DXA_VIDEODECODER},
    {"BrowserWatchdog", pbzero_enums::THREAD_BROWSER_WATCHDOG},
    {"WebRTC_Network", pbzero_enums::THREAD_WEBRTC_NETWORK},
    {"Window owner thread", pbzero_enums::THREAD_WINDOW_OWNER},
    {"WebRTC_Signaling", pbzero_enums::THREAD_WEBRTC_SIGNALING},
    {"GpuWatchdog", pbzero_enums::THREAD_GPU_WATCHDOG},
    {"swapper", pbzero_enums::THREAD_SWAPPER},
    {"Gamepad polling thread", pbzero_enums::THREAD_GAMEPAD_POLLING},
    {"AudioInputDevice", pbzero_enums::THREAD_AUDIO_INPUTDEVICE},
    {"WebRTC_Worker", pbzero_enums::THREAD_WEBRTC_WORKER},
    {"WebCrypto", pbzero_enums::THREAD_WEBCRYPTO},
    {"Database thread", pbzero_enums::THREAD_DATABASE},
    {"Proxy Resolver", pbzero_enums::THREAD_PROXYRESOLVER},
    {"Chrome_DevToolsADBThread", pbzero_enums::THREAD_DEVTOOLSADB},
    {"NetworkConfigWatcher", pbzero_enums::THREAD_NETWORKCONFIGWATCHER},
    {"wasapi_render_thread", pbzero_enums::THREAD_WASAPI_RENDER},
    {"LoaderLockSampler", pbzero_enums::THREAD_LOADER_LOCK_SAMPLER},
    {"CompositorGpuThread", pbzero_enums::THREAD_COMPOSITOR_GPU},
});

pbzero_enums::ThreadType GetThreadType(const char* const thread_name) {
  for (size_t i = 0; i < std::size(kThreadTypes); ++i) {
    if (base::MatchPattern(thread_name, kThreadTypes[i].name)) {
      return kThreadTypes[i].type;
    }
  }

  return pbzero_enums::THREAD_UNSPECIFIED;
}

}  // namespace tracing
