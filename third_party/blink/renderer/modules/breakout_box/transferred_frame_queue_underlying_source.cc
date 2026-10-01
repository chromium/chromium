// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/transferred_frame_queue_underlying_source.h"

#include "base/task/sequenced_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

template <typename NativeFrameType>
TransferredFrameQueueUnderlyingSource<NativeFrameType>::
    TransferredFrameQueueUnderlyingSource(
        ScriptState* script_state,
        scoped_refptr<FrameQueue<NativeFrameType>> queue,
        scoped_refptr<base::SequencedTaskRunner> host_runner,
        CrossThreadPersistent<FrameQueueHost> host,
        std::string device_id,
        wtf_size_t frame_pool_size,
        std::optional<base::ThreadType> thread_type)
    : FrameQueueUnderlyingSource<NativeFrameType>(script_state,
                                                  std::move(queue),
                                                  std::move(device_id),
                                                  frame_pool_size,
                                                  thread_type),
      host_runner_(std::move(host_runner)),
      host_(std::move(host)) {}

template <typename NativeFrameType>
bool TransferredFrameQueueUnderlyingSource<
    NativeFrameType>::StartFrameDelivery() {
  // PostCrossThreadTask needs a closure, so we have to ignore
  // StartFrameDelivery()'s return type.
  auto start_frame_delivery_cb = [](FrameQueueHost* host) {
    if (host) {
      host->StartFrameDelivery();
    }
  };

  PostCrossThreadTask(*host_runner_.get(), FROM_HERE,
                      CrossThreadBindOnce(start_frame_delivery_cb, host_));

  // This could fail on the host side, but for now we don't do an async check.
  return true;
}

template <typename NativeFrameType>
void TransferredFrameQueueUnderlyingSource<
    NativeFrameType>::StopFrameDelivery() {
  PostCrossThreadTask(*host_runner_.get(), FROM_HERE,
                      CrossThreadBindOnce(&FrameQueueHost::Close, host_));
}

template <typename NativeFrameType>
void TransferredFrameQueueUnderlyingSource<NativeFrameType>::Close() {
  if (this->IsClosed()) {
    return;
  }
  FrameQueueUnderlyingSource<NativeFrameType>::Close();
  if (host_runner_) {
    PostCrossThreadTask(
        *host_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(&FrameQueueHost::ClearTransferredSource, host_));
  }
}

template <typename NativeFrameType>
void TransferredFrameQueueUnderlyingSource<
    NativeFrameType>::ContextDestroyed() {
  FrameQueueUnderlyingSource<NativeFrameType>::ContextDestroyed();
}

template <typename NativeFrameType>
void TransferredFrameQueueUnderlyingSource<NativeFrameType>::Trace(
    Visitor* visitor) const {
  FrameQueueUnderlyingSource<NativeFrameType>::Trace(visitor);
}

template class MODULES_TEMPLATE_EXPORT
    TransferredFrameQueueUnderlyingSource<scoped_refptr<media::AudioBuffer>>;
template class MODULES_TEMPLATE_EXPORT
    TransferredFrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;

}  // namespace blink
