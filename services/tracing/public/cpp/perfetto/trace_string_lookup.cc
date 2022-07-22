// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_string_lookup.h"

#include "base/strings/pattern.h"

using ::perfetto::protos::pbzero::ChromeProcessDescriptor;
using ::perfetto::protos::pbzero::ChromeThreadDescriptor;

namespace tracing {

struct ProcessType {
  const char* name;
  ChromeProcessDescriptor::ProcessType type;
};

constexpr ProcessType kProcessTypes[] = {
    {"Browser", ChromeProcessDescriptor::PROCESS_BROWSER},
    {"Renderer", ChromeProcessDescriptor::PROCESS_RENDERER},
    {"Extension Renderer", ChromeProcessDescriptor::PROCESS_RENDERER_EXTENSION},
    {"GPU Process", ChromeProcessDescriptor::PROCESS_GPU},
    {"HeadlessBrowser", ChromeProcessDescriptor::PROCESS_BROWSER},
    {"PPAPI Process", ChromeProcessDescriptor::PROCESS_PPAPI_PLUGIN},
    {"PPAPI Broker Process", ChromeProcessDescriptor::PROCESS_PPAPI_BROKER},
    {"Service: network.mojom.NetworkService",
     ChromeProcessDescriptor::PROCESS_SERVICE_NETWORK},
    {"Service: tracing.mojom.TracingService",
     ChromeProcessDescriptor::PROCESS_SERVICE_TRACING},
    {"Service: storage.mojom.StorageService",
     ChromeProcessDescriptor::PROCESS_SERVICE_STORAGE},
    {"Service: audio.mojom.AudioService",
     ChromeProcessDescriptor::PROCESS_SERVICE_AUDIO},
    {"Service: data_decoder.mojom.DataDecoderService",
     ChromeProcessDescriptor::PROCESS_SERVICE_DATA_DECODER},
    {"Service: chrome.mojom.UtilWin",
     ChromeProcessDescriptor::PROCESS_SERVICE_UTIL_WIN},
    {"Service: proxy_resolver.mojom.ProxyResolverFactory",
     ChromeProcessDescriptor::PROCESS_SERVICE_PROXY_RESOLVER},
    {"Service: media.mojom.CdmService",
     ChromeProcessDescriptor::PROCESS_SERVICE_CDM},
    {"Service: video_capture.mojom.VideoCaptureService",
     ChromeProcessDescriptor::PROCESS_SERVICE_VIDEO_CAPTURE},
    {"Service: unzip.mojom.Unzipper",
     ChromeProcessDescriptor::PROCESS_SERVICE_UNZIPPER},
    {"Service: mirroring.mojom.MirroringService",
     ChromeProcessDescriptor::PROCESS_SERVICE_MIRRORING},
    {"Service: patch.mojom.FilePatcher",
     ChromeProcessDescriptor::PROCESS_SERVICE_FILEPATCHER},
    {"Service: chromeos.tts.mojom.TtsService",
     ChromeProcessDescriptor::PROCESS_SERVICE_TTS},
    {"Service: printing.mojom.PrintingService",
     ChromeProcessDescriptor::PROCESS_SERVICE_PRINTING},
    {"Service: quarantine.mojom.Quarantine",
     ChromeProcessDescriptor::PROCESS_SERVICE_QUARANTINE},
    {"Service: chromeos.local_search_service.mojom.LocalSearchService",
     ChromeProcessDescriptor::PROCESS_SERVICE_CROS_LOCALSEARCH},
    {"Service: ash.assistant.mojom.AssistantAudioDecoderFactory",
     ChromeProcessDescriptor::PROCESS_SERVICE_CROS_ASSISTANT_AUDIO_DECODER},
    {"Service: chrome.mojom.FileUtilService",
     ChromeProcessDescriptor::PROCESS_SERVICE_FILEUTIL},
    {"Service: printing.mojom.PrintCompositor",
     ChromeProcessDescriptor::PROCESS_SERVICE_PRINTCOMPOSITOR},
    {"Service: paint_preview.mojom.PaintPreviewCompositorCollection",
     ChromeProcessDescriptor::PROCESS_SERVICE_PAINTPREVIEW},
    {"Service: media.mojom.SpeechRecognitionService",
     ChromeProcessDescriptor::PROCESS_SERVICE_SPEECHRECOGNITION},
    {"Service: device.mojom.XRDeviceService",
     ChromeProcessDescriptor::PROCESS_SERVICE_XRDEVICE},
    {"Service: chrome.mojom.UtilReadIcon",
     ChromeProcessDescriptor::PROCESS_SERVICE_READICON},
    {"Service: language_detection.mojom.LanguageDetectionService",
     ChromeProcessDescriptor::PROCESS_SERVICE_LANGUAGEDETECTION},
    {"Service: sharing.mojom.Sharing",
     ChromeProcessDescriptor::PROCESS_SERVICE_SHARING},
    {"Service: chrome.mojom.MediaParserFactory",
     ChromeProcessDescriptor::PROCESS_SERVICE_MEDIAPARSER},
    {"Service: qrcode_generator.mojom.QRCodeGeneratorService",
     ChromeProcessDescriptor::PROCESS_SERVICE_QRCODEGENERATOR},
    {"Service: chrome.mojom.ProfileImport",
     ChromeProcessDescriptor::PROCESS_SERVICE_PROFILEIMPORT},
    {"Service: ash.ime.mojom.ImeService",
     ChromeProcessDescriptor::PROCESS_SERVICE_IME},
    {"Service: recording.mojom.RecordingService",
     ChromeProcessDescriptor::PROCESS_SERVICE_RECORDING},
    {"Service: shape_detection.mojom.ShapeDetectionService",
     ChromeProcessDescriptor::PROCESS_SERVICE_SHAPEDETECTION},
    // Catch-all, should be after all the "Service:" entries.
    {"Service:*", ChromeProcessDescriptor::PROCESS_UTILITY},
};

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

ChromeProcessDescriptor::ProcessType GetProcessType(const std::string& name) {
  for (size_t i = 0; i < std::size(kProcessTypes); ++i) {
    if (base::MatchPattern(name, kProcessTypes[i].name)) {
      return kProcessTypes[i].type;
    }
  }

  return ChromeProcessDescriptor::PROCESS_UNSPECIFIED;
}

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
