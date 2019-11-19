// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/memory_handler.h"

#include <cinttypes>

#include "base/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"

namespace content {
namespace protocol {

MemoryHandler::MemoryHandler()
    : DevToolsDomainHandler(Memory::Metainfo::domainName),
      process_host_id_(ChildProcessHost::kInvalidUniqueID) {}

MemoryHandler::~MemoryHandler() {}

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
  return Response::OK();
}

Response MemoryHandler::SetPressureNotificationsSuppressed(
    bool suppressed) {
  base::MemoryPressureListener::SetNotificationsSuppressed(suppressed);
  return Response::OK();
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
  return Response::OK();
}

void MemoryHandler::PrepareForLeakDetection(
    std::unique_ptr<PrepareForLeakDetectionCallback> callback) {
  if (leak_detection_callback_) {
    callback->sendFailure(
        Response::Error("Another leak detection in progress"));
    return;
  }
  RenderProcessHost* process = RenderProcessHost::FromID(process_host_id_);
  if (!process) {
    callback->sendFailure(Response::Error("No process to detect leaks in"));
    return;
  }

  leak_detection_callback_ = std::move(callback);
  process->BindReceiver(leak_detector_.BindNewPipeAndPassReceiver());
  leak_detector_.set_disconnect_handler(base::BindOnce(
      &MemoryHandler::OnLeakDetectorIsGone, base::Unretained(this)));
  leak_detector_->PerformLeakDetection(base::BindOnce(
      &MemoryHandler::OnLeakDetectionComplete, weak_factory_.GetWeakPtr()));
}

void MemoryHandler::OnLeakDetectionComplete(
    blink::mojom::LeakDetectionResultPtr result) {
  leak_detection_callback_->sendSuccess();
  leak_detection_callback_.reset();
  leak_detector_.reset();
}

void MemoryHandler::OnLeakDetectorIsGone() {
  leak_detection_callback_->sendFailure(
      Response::Error("Failed to run leak detection"));
  leak_detection_callback_.reset();
  leak_detector_.reset();
}

}  // namespace protocol
}  // namespace content
