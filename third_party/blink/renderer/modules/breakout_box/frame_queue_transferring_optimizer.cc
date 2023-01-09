// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/frame_queue_transferring_optimizer.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/modules/breakout_box/transferred_frame_queue_underlying_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

template <typename NativeFrameType>
FrameQueueTransferringOptimizer<NativeFrameType>::
    FrameQueueTransferringOptimizer(
        FrameQueueHost* host,
        scoped_refptr<base::SequencedTaskRunner> host_runner,
        wtf_size_t max_queue_size,
        ConnectHostCallback connect_host_callback,
        CrossThreadOnceClosure transferred_source_destroyed_callback)
    : host_(host),
      host_runner_(std::move(host_runner)),
      connect_host_callback_(std::move(connect_host_callback)),
      transferred_source_destroyed_callback_(
          std::move(transferred_source_destroyed_callback)),
      max_queue_size_(max_queue_size) {}

template <typename NativeFrameType>
UnderlyingSourceBase*
FrameQueueTransferringOptimizer<NativeFrameType>::PerformInProcessOptimization(
    ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  scoped_refptr<base::SingleThreadTaskRunner> current_runner =
      context->GetTaskRunner(TaskType::kInternalMediaRealTime);

  auto host = host_.Lock();
  if (!host)
    return nullptr;

  auto* source = MakeGarbageCollected<
      TransferredFrameQueueUnderlyingSource<NativeFrameType>>(
      script_state, host, host_runner_,
      std::move(transferred_source_destroyed_callback_));

  PostCrossThreadTask(
      *host_runner_, FROM_HERE,
      CrossThreadBindOnce(std::move(connect_host_callback_), current_runner,
                          WrapCrossThreadPersistent(source)));
  return source;
}

template class MODULES_TEMPLATE_EXPORT
    FrameQueueTransferringOptimizer<scoped_refptr<media::AudioBuffer>>;
template class MODULES_TEMPLATE_EXPORT
    FrameQueueTransferringOptimizer<scoped_refptr<media::VideoFrame>>;

}  // namespace blink
