// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/profiler_trace_builder.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_marker.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_sample.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_stack.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_trace.h"
#include "third_party/blink/renderer/core/timing/performance.h"
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
  if (profile) {
    for (int i = 0; i < profile->GetSamplesCount(); i++) {
      const auto* node = profile->GetSample(i);
      auto timestamp = base::TimeTicks() +
                       base::Microseconds(profile->GetSampleTimestamp(i));
      const auto state = profile->GetSampleState(i);
      const auto embedder_state = profile->GetSampleEmbedderState(i);
      builder->AddSample(node, timestamp, state, embedder_state);
    }
  }
  return builder->GetTrace();
}

ProfilerTraceBuilder::ProfilerTraceBuilder(ScriptState* script_state,
                                           const SecurityOrigin* allowed_origin,
                                           base::TimeTicks time_origin)
    : script_state_(script_state),
      allowed_origin_(allowed_origin),
      time_origin_(time_origin) {}

void ProfilerTraceBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(frames_);
  visitor->Trace(stacks_);
  visitor->Trace(samples_);
}

void ProfilerTraceBuilder::AddSample(
    const v8::CpuProfileNode* node,
    base::TimeTicks timestamp,
    const v8::StateTag state,
    const v8::EmbedderStateTag embedder_state) {
  auto* sample = ProfilerSample::Create();
  // TODO(yoav): This should not use MonotonicTimeToDOMHighResTimeStamp, as
  // these timestamps are clamped, which makes no sense for traces. Since this
  // only exposes time to traces, it's fine to define this as statically "cross
  // origin isolated".
  auto relative_timestamp = Performance::MonotonicTimeToDOMHighResTimeStamp(
      time_origin_, timestamp, /*allow_negative_value=*/true,
      /*cross_origin_isolated_capability=*/true);

  sample->setTimestamp(relative_timestamp);
  if (std::optional<wtf_size_t> stack_id = GetOrInsertStackId(node)) {
    sample->setStackId(*stack_id);
  }

  if (std::optional<blink::V8ProfilerMarker> marker =
          BlinkStateToMarker(embedder_state, state)) {
    sample->setMarker(*marker);
  }

  samples_.push_back(sample);
}

std::optional<wtf_size_t> ProfilerTraceBuilder::GetOrInsertStackId(
    const v8::CpuProfileNode* node) {
  if (!node)
    return std::optional<wtf_size_t>();

  if (!ShouldIncludeStackFrame(node))
    return GetOrInsertStackId(node->GetParent());

  auto existing_stack_id = node_to_stack_map_.find(node);
  if (existing_stack_id != node_to_stack_map_.end()) {
    // If we found a stack entry for this node ID, the subpath to the root
    // already exists in the trace, and we may coalesce.
    return existing_stack_id->value;
  }

  auto* stack = ProfilerStack::Create();
  wtf_size_t frame_id = GetOrInsertFrameId(node);
  stack->setFrameId(frame_id);
  if (std::optional<int> parent_stack_id =
          GetOrInsertStackId(node->GetParent())) {
    stack->setParentId(*parent_stack_id);
  }

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
    const v8::CpuProfileNode* node) {
  DCHECK(node);

  // Omit V8 metadata frames.
  const v8::CpuProfileNode::SourceType source_type = node->GetSourceType();
  if (source_type != v8::CpuProfileNode::kScript &&
      source_type != v8::CpuProfileNode::kBuiltin &&
      source_type != v8::CpuProfileNode::kCallback) {
    return false;
  }

  // Attempt to attribute each stack frame to a script.
  // - For JS functions, this is their own script.
  // - For builtins, this is the first attributable caller script.
  const v8::CpuProfileNode* resource_node = node;
  if (source_type != v8::CpuProfileNode::kScript) {
    while (resource_node &&
           resource_node->GetScriptId() == v8::UnboundScript::kNoScriptId) {
      resource_node = resource_node->GetParent();
    }
  }
  if (!resource_node)
    return false;

  int script_id = resource_node->GetScriptId();

  // If we already tested whether or not this script was cross-origin, return
  // the cached results.
  auto it = script_same_origin_cache_.find(script_id);
  if (it != script_same_origin_cache_.end())
    return it->value;

  KURL resource_url(resource_node->GetScriptResourceNameStr());
  if (!resource_url.IsValid())
    return false;

  auto origin = SecurityOrigin::Create(resource_url);
  // Omit frames that don't pass a cross-origin check.
  // Do this at the stack level (rather than the frame level) to avoid
  // including skeleton frames without data.
  bool allowed = resource_node->IsScriptSharedCrossOrigin() ||
                 origin->IsSameOriginWith(allowed_origin_);
  script_same_origin_cache_.Set(script_id, allowed);
  return allowed;
}

}  // namespace blink
