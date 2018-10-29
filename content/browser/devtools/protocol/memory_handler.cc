// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/memory_handler.h"

#include <cinttypes>

#include "base/memory/memory_pressure_listener.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_features.h"

namespace content {
namespace protocol {

MemoryHandler::MemoryHandler()
    : DevToolsDomainHandler(Memory::Metainfo::domainName),
      process_host_id_(ChildProcessHost::kInvalidUniqueID),
      weak_factory_(this) {}

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
  std::unique_ptr<Array<Memory::SamplingProfileNode>> samples =
      Array<Memory::SamplingProfileNode>::create();
  std::vector<base::SamplingHeapProfiler::Sample> raw_samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);

  for (auto& sample : raw_samples) {
    std::unique_ptr<Array<String>> stack = Array<String>::create();
    for (const void* frame : sample.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      module_cache.GetModuleForAddress(address);  // Populates module_cache.
      stack->addItem(base::StringPrintf("0x%" PRIxPTR, address));
    }
    samples->addItem(Memory::SamplingProfileNode::Create()
                         .SetSize(sample.size)
                         .SetTotal(sample.total)
                         .SetStack(std::move(stack))
                         .Build());
  }

  std::unique_ptr<Array<Memory::Module>> modules =
      Array<Memory::Module>::create();
  for (const auto* module : module_cache.GetModules()) {
    modules->addItem(Memory::Module::Create()
                         .SetName(base::StringPrintf(
                             "%" PRFilePath, module->filename.value().c_str()))
                         .SetUuid(module->id)
                         .SetBaseAddress(base::StringPrintf(
                             "0x%" PRIxPTR, module->base_address))
                         .SetSize(static_cast<double>(module->size))
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
  if (base::FeatureList::IsEnabled(features::kMemoryCoordinator)) {
    return Response::Error(
        "Cannot enable/disable notifications when memory coordinator is "
        "enabled");
  }

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
  BindInterface(process, &leak_detector_);
  leak_detector_.set_connection_error_handler(base::BindOnce(
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
