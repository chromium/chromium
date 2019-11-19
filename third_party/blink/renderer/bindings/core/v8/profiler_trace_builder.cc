// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/profiler_trace_builder.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/profiler_frame.h"
#include "third_party/blink/renderer/core/timing/profiler_sample.h"
#include "third_party/blink/renderer/core/timing/profiler_stack.h"
#include "third_party/blink/renderer/core/timing/profiler_trace.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

ProfilerTrace* ProfilerTraceBuilder::FromProfile(
    ScriptState* script_state,
    const v8::CpuProfile* profile,
    const SecurityOrigin* allowed_origin,
    base::TimeTicks time_origin) {
  TRACE_EVENT0("blink", "ProfilerTraceBuilder::FromProfile");
  ProfilerTraceBuilder* builder = MakeGarbageCollected<ProfilerTraceBuilder>(
      script_state, allowed_origin, time_origin);
  for (int i = 0; i < profile->GetSamplesCount(); i++) {
    const auto* node = profile->GetSample(i);
    auto timestamp = base::TimeTicks() + base::TimeDelta::FromMicroseconds(
                                             profile->GetSampleTimestamp(i));
    builder->AddSample(node, timestamp);
  }
  return builder->GetTrace();
}

ProfilerTraceBuilder::ProfilerTraceBuilder(ScriptState* script_state,
                                           const SecurityOrigin* allowed_origin,
                                           base::TimeTicks time_origin)
    : script_state_(script_state),
      allowed_origin_(allowed_origin),
      time_origin_(time_origin) {}

void ProfilerTraceBuilder::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(frames_);
  visitor->Trace(stacks_);
  visitor->Trace(samples_);
}

void ProfilerTraceBuilder::AddSample(const v8::CpuProfileNode* node,
                                     base::TimeTicks timestamp) {
  auto* sample = ProfilerSample::Create();
  auto relative_timestamp = Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timestamp, true);

  sample->setTimestamp(relative_timestamp);
  if (base::Optional<wtf_size_t> stack_id = GetOrInsertStackId(node))
    sample->setStackId(*stack_id);

  samples_.push_back(sample);
}

base::Optional<wtf_size_t> ProfilerTraceBuilder::GetOrInsertStackId(
    const v8::CpuProfileNode* node) {
  if (!node)
    return base::Optional<wtf_size_t>();

  // Omit frames that don't pass a cross-origin check.
  // Do this at the stack level (rather than the frame level) to avoid
  // including skeleton frames without data.
  KURL resource_url(node->GetScriptResourceNameStr());
  if (!ShouldIncludeStackFrame(resource_url, node->GetScriptId(),
                               node->GetSourceType(),
                               node->IsScriptSharedCrossOrigin())) {
    return GetOrInsertStackId(node->GetParent());
  }

  auto existing_stack_id = node_to_stack_map_.find(node);
  if (existing_stack_id != node_to_stack_map_.end()) {
    // If we found a stack entry for this node ID, the subpath to the root
    // already exists in the trace, and we may coalesce.
    return existing_stack_id->value;
  }

  auto* stack = ProfilerStack::Create();
  wtf_size_t frame_id = GetOrInsertFrameId(node);
  stack->setFrameId(frame_id);
  if (base::Optional<int> parent_stack_id =
          GetOrInsertStackId(node->GetParent()))
    stack->setParentId(*parent_stack_id);

  wtf_size_t stack_id = stacks_.size();
  stacks_.push_back(stack);
  node_to_stack_map_.Set(node, stack_id);
  return stack_id;
}

wtf_size_t ProfilerTraceBuilder::GetOrInsertFrameId(
    const v8::CpuProfileNode* node) {
  auto existing_frame_id = node_to_frame_map_.find(node);

  if (existing_frame_id != node_to_frame_map_.end())
    return existing_frame_id->value;

  auto* frame = ProfilerFrame::Create();
  frame->setName(node->GetFunctionNameStr());
  if (*node->GetScriptResourceNameStr() != '\0') {
    wtf_size_t resource_id =
        GetOrInsertResourceId(node->GetScriptResourceNameStr());
    frame->setResourceId(resource_id);
  }
  if (node->GetLineNumber() != v8::CpuProfileNode::kNoLineNumberInfo)
    frame->setLine(node->GetLineNumber());
  if (node->GetColumnNumber() != v8::CpuProfileNode::kNoColumnNumberInfo)
    frame->setColumn(node->GetColumnNumber());

  wtf_size_t frame_id = frames_.size();
  frames_.push_back(frame);
  node_to_frame_map_.Set(node, frame_id);

  return frame_id;
}

wtf_size_t ProfilerTraceBuilder::GetOrInsertResourceId(
    const char* resource_name) {
  // Since V8's CPU profiler already does string interning, pointer equality is
  // value equality here.
  auto existing_resource_id = resource_map_.find(resource_name);

  if (existing_resource_id != resource_map_.end())
    return existing_resource_id->value;

  wtf_size_t resource_id = resources_.size();
  resources_.push_back(resource_name);
  resource_map_.Set(resource_name, resource_id);

  return resource_id;
}

ProfilerTrace* ProfilerTraceBuilder::GetTrace() const {
  ProfilerTrace* trace = ProfilerTrace::Create();
  trace->setResources(resources_);
  trace->setFrames(frames_);
  trace->setStacks(stacks_);
  trace->setSamples(samples_);
  return trace;
}

bool ProfilerTraceBuilder::ShouldIncludeStackFrame(
    const KURL& script_url,
    int script_id,
    v8::CpuProfileNode::SourceType source_type,
    bool script_shared_cross_origin) {
  // Omit V8 metadata frames.
  if (source_type != v8::CpuProfileNode::kScript &&
      source_type != v8::CpuProfileNode::kBuiltin &&
      source_type != v8::CpuProfileNode::kCallback) {
    return false;
  }

  // If we couldn't derive script data, only allow builtins and callbacks.
  if (script_id == v8::UnboundScript::kNoScriptId) {
    return source_type == v8::CpuProfileNode::kBuiltin ||
           source_type == v8::CpuProfileNode::kCallback;
  }

  // If we already tested whether or not this script was cross-origin, return
  // the cached results.
  auto it = script_same_origin_cache_.find(script_id);
  if (it != script_same_origin_cache_.end())
    return it->value;

  if (!script_url.IsValid())
    return false;

  auto origin = SecurityOrigin::Create(script_url);
  // TODO(acomminos): Consider easing this check based on optional headers.
  bool allowed = script_shared_cross_origin ||
                 origin->IsSameSchemeHostPort(allowed_origin_);
  script_same_origin_cache_.Set(script_id, allowed);
  return allowed;
}

}  // namespace blink
