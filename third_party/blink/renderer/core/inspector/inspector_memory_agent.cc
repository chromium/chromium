/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/inspector_memory_agent.h"

#include <cstdio>

#include "base/debug/stack_trace.h"
#include "base/profiler/module_cache.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

constexpr int kDefaultNativeMemorySamplingInterval = 128 * 1024;

InspectorMemoryAgent::InspectorMemoryAgent(InspectedFrames* inspected_frames)
    : frames_(inspected_frames),
      sampling_profile_interval_(&agent_state_, /*default_value=*/0) {}

InspectorMemoryAgent::~InspectorMemoryAgent() = default;

protocol::Response InspectorMemoryAgent::getDOMCounters(
    int* documents,
    int* nodes,
    int* js_event_listeners) {
  *documents =
      InstanceCounters::CounterValue(InstanceCounters::kDocumentCounter);
  *nodes = InstanceCounters::CounterValue(InstanceCounters::kNodeCounter);
  *js_event_listeners =
      InstanceCounters::CounterValue(InstanceCounters::kJSEventListenerCounter);
  return protocol::Response::Success();
}

protocol::Response InspectorMemoryAgent::forciblyPurgeJavaScriptMemory() {
  for (const auto& page : Page::OrdinaryPages()) {
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      LocalFrame* local_frame = DynamicTo<LocalFrame>(frame);
      if (!local_frame)
        continue;
      local_frame->ForciblyPurgeV8Memory();
    }
  }
  v8::Isolate* isolate =
      frames_->Root()->GetPage()->GetAgentGroupScheduler().Isolate();
  isolate->MemoryPressureNotification(v8::MemoryPressureLevel::kCritical);
  return protocol::Response::Success();
}

void InspectorMemoryAgent::Trace(Visitor* visitor) const {
  visitor->Trace(frames_);
  InspectorBaseAgent::Trace(visitor);
}

void InspectorMemoryAgent::Restore() {
  // The action below won't start sampling if the sampling_interval is zero.
  startSampling(protocol::Maybe<int>(sampling_profile_interval_.Get()),
                protocol::Maybe<bool>());
}

protocol::Response InspectorMemoryAgent::startSampling(
    protocol::Maybe<int> in_sampling_interval,
    protocol::Maybe<bool> in_suppressRandomness) {
  int interval =
      in_sampling_interval.value_or(kDefaultNativeMemorySamplingInterval);
  if (interval <= 0)
    return protocol::Response::ServerError("Invalid sampling rate.");
  base::SamplingHeapProfiler::Get()->SetSamplingInterval(interval);
  sampling_profile_interval_.Set(interval);
  if (in_suppressRandomness.value_or(false)) {
    randomness_suppressor_ = std::make_unique<
        base::PoissonAllocationSampler::ScopedSuppressRandomnessForTesting>();
  }
  profile_id_ = base::SamplingHeapProfiler::Get()->Start();
  return protocol::Response::Success();
}

protocol::Response InspectorMemoryAgent::stopSampling() {
  if (sampling_profile_interval_.Get() == 0)
    return protocol::Response::ServerError("Sampling profiler is not started.");
  base::SamplingHeapProfiler::Get()->Stop();
  sampling_profile_interval_.Clear();
  randomness_suppressor_.reset();
  return protocol::Response::Success();
}

protocol::Response InspectorMemoryAgent::getAllTimeSamplingProfile(
    std::unique_ptr<protocol::Memory::SamplingProfile>* out_profile) {
  *out_profile = GetSamplingProfileById(0);
  return protocol::Response::Success();
}

protocol::Response InspectorMemoryAgent::getSamplingProfile(
    std::unique_ptr<protocol::Memory::SamplingProfile>* out_profile) {
  *out_profile = GetSamplingProfileById(profile_id_);
  return protocol::Response::Success();
}

std::unique_ptr<protocol::Memory::SamplingProfile>
InspectorMemoryAgent::GetSamplingProfileById(uint32_t id) {
  base::ModuleCache module_cache;
  auto samples = std::make_unique<
      protocol::Array<protocol::Memory::SamplingProfileNode>>();
  auto raw_samples = base::SamplingHeapProfiler::Get()->GetSamples(id);

  for (auto& it : raw_samples) {
    for (const void* frame : it.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      module_cache.GetModuleForAddress(address);  // Populates module_cache.
    }
    Vector<String> source_stack = Symbolize(it.stack);
    auto stack = std::make_unique<protocol::Array<protocol::String>>();
    for (const auto& frame : source_stack)
      stack->emplace_back(frame);
    samples->emplace_back(protocol::Memory::SamplingProfileNode::create()
                              .setSize(it.size)
                              .setTotal(it.total)
                              .setStack(std::move(stack))
                              .build());
  }

  // Mix in v8 main isolate heap size as a synthetic node.
  // TODO(alph): Add workers' heap sizes.
  if (!id) {
    v8::HeapStatistics heap_stats;
    v8::Isolate* isolate =
        frames_->Root()->GetPage()->GetAgentGroupScheduler().Isolate();
    isolate->GetHeapStatistics(&heap_stats);
    size_t total_bytes = heap_stats.total_heap_size();
    auto stack = std::make_unique<protocol::Array<protocol::String>>();
    stack->emplace_back("<V8 Heap>");
    samples->emplace_back(protocol::Memory::SamplingProfileNode::create()
                              .setSize(total_bytes)
                              .setTotal(total_bytes)
                              .setStack(std::move(stack))
                              .build());
  }

  auto modules = std::make_unique<protocol::Array<protocol::Memory::Module>>();
  for (const auto* module : module_cache.GetModules()) {
    modules->emplace_back(
        protocol::Memory::Module::create()
            .setName(module->GetDebugBasename().AsUTF16Unsafe().c_str())
            .setUuid(module->GetId().c_str())
            .setBaseAddress(
                String::Format("0x%" PRIxPTR, module->GetBaseAddress()))
            .setSize(static_cast<double>(module->GetSize()))
            .build());
  }

  return protocol::Memory::SamplingProfile::create()
      .setSamples(std::move(samples))
      .setModules(std::move(modules))
      .build();
}

Vector<String> InspectorMemoryAgent::Symbolize(
    const WebVector<const void*>& addresses) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // TODO(alph): Move symbolization to the client.
  Vector<const void*> addresses_to_symbolize;
  for (const void* address : addresses) {
    if (!symbols_cache_.Contains(address)) {
      addresses_to_symbolize.push_back(address);
    }
  }

  String text(
      base::debug::StackTrace(addresses_to_symbolize).ToString().c_str());
  // Populate cache with new entries.
  wtf_size_t next_pos;
  for (wtf_size_t pos = 0, i = 0;; pos = next_pos + 1, ++i) {
    next_pos = text.find('\n', pos);
    if (next_pos == kNotFound)
      break;
    String line = text.Substring(pos, next_pos - pos);
    wtf_size_t space_pos = line.ReverseFind(' ');
    String name = line.Substring(space_pos == kNotFound ? 0 : space_pos + 1);
    symbols_cache_.insert(addresses_to_symbolize[i], name);
  }
#endif

  Vector<String> result;
  for (const void* address : addresses) {
    char buffer[20];
    std::snprintf(buffer, sizeof(buffer), "0x%" PRIxPTR,
                  reinterpret_cast<uintptr_t>(address));
    if (symbols_cache_.Contains(address)) {
      StringBuilder builder;
      builder.Append(buffer);
      builder.Append(" ");
      builder.Append(symbols_cache_.at(address));
      result.push_back(builder.ToString());
    } else {
      result.push_back(buffer);
    }
  }
  return result;
}

}  // namespace blink
