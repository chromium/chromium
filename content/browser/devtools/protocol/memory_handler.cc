// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/memory_handler.h"

#include <cinttypes>

#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/profiler/module_cache.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"

namespace content {
namespace protocol {

namespace {
constexpr char kAnotherLeakDetectionInProgress[] =
    "Another leak detection in progress";
constexpr char kNoProcessToDetectLeaksIn[] = "No process to detect leaks in";
constexpr char kFailedToRunLeakDetection[] = "Failed to run leak detection";
}  // namespace

MemoryHandler::MemoryHandler()
    : DevToolsDomainHandler(Memory::Metainfo::domainName),
      process_host_id_(ChildProcessHost::kInvalidUniqueID) {}

MemoryHandler::~MemoryHandler() = default;

void MemoryHandler::Wire(UberDispatcher* dispatcher) {
  Memory::Dispatcher::wire(dispatcher, this);
}

void MemoryHandler::SetRenderer(int process_host_id,
                                RenderFrameHostImpl* frame_host) {
  process_host_id_ = process_host_id;
}

Response MemoryHandler::GetBrowserSamplingProfile(
    std::unique_ptr<Memory::SamplingProfile>* out_profile) {
  base::ModuleCache module_cache;
  auto samples = std::make_unique<Array<Memory::SamplingProfileNode>>();
  std::vector<base::SamplingHeapProfiler::Sample> raw_samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);

  for (auto& sample : raw_samples) {
    auto stack = std::make_unique<Array<String>>();
    for (const void* frame : sample.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      module_cache.GetModuleForAddress(address);  // Populates module_cache.
      stack->emplace_back(base::StringPrintf("0x%" PRIxPTR, address));
    }
    samples->emplace_back(Memory::SamplingProfileNode::Create()
                              .SetSize(sample.size)
                              .SetTotal(sample.total)
                              .SetStack(std::move(stack))
                              .Build());
  }

  auto modules = std::make_unique<Array<Memory::Module>>();
  for (const auto* module : module_cache.GetModules()) {
    modules->emplace_back(
        Memory::Module::Create()
            .SetName(base::StringPrintf(
                "%" PRFilePath, module->GetDebugBasename().value().c_str()))
            .SetUuid(module->GetId())
            .SetBaseAddress(
                base::StringPrintf("0x%" PRIxPTR, module->GetBaseAddress()))
            .SetSize(static_cast<double>(module->GetSize()))
            .Build());
  }

  *out_profile = Memory::SamplingProfile::Create()
                     .SetSamples(std::move(samples))
                     .SetModules(std::move(modules))
                     .Build();
  return Response::Success();
}

Response MemoryHandler::SetPressureNotificationsSuppressed(
    bool suppressed) {
  base::MemoryPressureListener::SetNotificationsSuppressed(suppressed);
  return Response::Success();
}

Response MemoryHandler::SimulatePressureNotification(
    const std::string& level) {
  base::MemoryPressureListener::MemoryPressureLevel parsed_level;
  if (level == protocol::Memory::PressureLevelEnum::Moderate) {
    parsed_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  } else if (level == protocol::Memory::PressureLevelEnum::Critical) {
    parsed_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  } else {
    return Response::InvalidParams(base::StringPrintf(
        "Invalid memory pressure level '%s'", level.c_str()));
  }

  // Simulate memory pressure notification in the browser process.
  base::MemoryPressureListener::SimulatePressureNotification(parsed_level);
  return Response::Success();
}

void MemoryHandler::PrepareForLeakDetection(
    std::unique_ptr<PrepareForLeakDetectionCallback> callback) {
  if (prepare_for_leak_detection_callback_ ||
      get_dom_counters_for_leak_detection_callback_) {
    callback->sendFailure(
        Response::ServerError(kAnotherLeakDetectionInProgress));
    return;
  }

  RenderProcessHost* process = RenderProcessHost::FromID(process_host_id_);
  if (!process) {
    callback->sendFailure(Response::ServerError(kNoProcessToDetectLeaksIn));
    return;
  }

  prepare_for_leak_detection_callback_ = std::move(callback);

  RequestLeakDetection(process);
}

void MemoryHandler::GetDOMCountersForLeakDetection(
    std::unique_ptr<GetDOMCountersForLeakDetectionCallback> callback) {
  if (prepare_for_leak_detection_callback_ ||
      get_dom_counters_for_leak_detection_callback_) {
    callback->sendFailure(
        Response::ServerError(kAnotherLeakDetectionInProgress));
    return;
  }

  RenderProcessHost* process = RenderProcessHost::FromID(process_host_id_);
  if (!process) {
    callback->sendFailure(Response::ServerError(kNoProcessToDetectLeaksIn));
    return;
  }

  get_dom_counters_for_leak_detection_callback_ = std::move(callback);

  RequestLeakDetection(process);
}

void MemoryHandler::RequestLeakDetection(RenderProcessHost* process) {
  process->BindReceiver(leak_detector_.BindNewPipeAndPassReceiver());

  // Using base::Unretained(this) in the code below is safe because the
  // callbacks are passed to the mojo remote member, so they are guaranteed
  // not to survive this.
  leak_detector_.set_disconnect_handler(base::BindOnce(
      &MemoryHandler::OnLeakDetectorIsGone, base::Unretained(this)));

  leak_detector_->PerformLeakDetection(base::BindOnce(
      &MemoryHandler::OnLeakDetectionComplete, base::Unretained(this)));
}

void MemoryHandler::OnLeakDetectionComplete(
    blink::mojom::LeakDetectionResultPtr result) {
  if (prepare_for_leak_detection_callback_) {
    prepare_for_leak_detection_callback_->sendSuccess();
    prepare_for_leak_detection_callback_.reset();
  }
  if (get_dom_counters_for_leak_detection_callback_) {
    if (result) {
      get_dom_counters_for_leak_detection_callback_->sendSuccess(
          GetDOMCounters(*result.get()));
    } else {
      get_dom_counters_for_leak_detection_callback_->sendFailure(
          Response::ServerError(kFailedToRunLeakDetection));
    }
    get_dom_counters_for_leak_detection_callback_.reset();
  }
  leak_detector_.reset();
}

void MemoryHandler::OnLeakDetectorIsGone() {
  if (prepare_for_leak_detection_callback_) {
    prepare_for_leak_detection_callback_->sendFailure(
        Response::ServerError(kFailedToRunLeakDetection));
    prepare_for_leak_detection_callback_.reset();
  }
  if (get_dom_counters_for_leak_detection_callback_) {
    get_dom_counters_for_leak_detection_callback_->sendFailure(
        Response::ServerError(kFailedToRunLeakDetection));
    get_dom_counters_for_leak_detection_callback_.reset();
  }
  leak_detector_.reset();
}

std::unique_ptr<protocol::Array<protocol::Memory::DOMCounter>>
MemoryHandler::GetDOMCounters(const blink::mojom::LeakDetectionResult& result) {
  auto counters =
      std::make_unique<protocol::Array<protocol::Memory::DOMCounter>>();

#define ADD_COUNTER(name)                                       \
  counters->emplace_back(protocol::Memory::DOMCounter::Create() \
                             .SetName(#name)                    \
                             .SetCount(result.number_of_##name) \
                             .Build());
  ADD_COUNTER(live_audio_nodes)
  ADD_COUNTER(live_documents)
  ADD_COUNTER(live_nodes)
  ADD_COUNTER(live_layout_objects)
  ADD_COUNTER(live_resources)
  ADD_COUNTER(live_context_lifecycle_state_observers)
  ADD_COUNTER(live_frames)
  ADD_COUNTER(live_v8_per_context_data)
  ADD_COUNTER(worker_global_scopes)
  ADD_COUNTER(live_ua_css_resources)
  ADD_COUNTER(live_resource_fetchers)

#undef ADD_COUNTER

  return counters;
}

}  // namespace protocol
}  // namespace content
