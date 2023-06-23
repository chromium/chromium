// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_sender_source_optimizer.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

RtcEncodedVideoSenderSourceOptimizer::RtcEncodedVideoSenderSourceOptimizer(
    UnderlyingSourceSetter set_underlying_source,
    WTF::CrossThreadOnceClosure disconnect_callback)
    : set_underlying_source_(std::move(set_underlying_source)),
      disconnect_callback_(std::move(disconnect_callback)) {}

UnderlyingSourceBase*
RtcEncodedVideoSenderSourceOptimizer::PerformInProcessOptimization(
    ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  scoped_refptr<base::SingleThreadTaskRunner> current_runner =
      context->GetTaskRunner(TaskType::kInternalMediaRealTime);

  auto* new_source = MakeGarbageCollected<RTCEncodedVideoUnderlyingSource>(
      script_state, std::move(disconnect_callback_));

  set_underlying_source_.Run(WrapCrossThreadPersistent(new_source),
                             std::move(current_runner));

  return new_source;
}

}  // namespace blink
