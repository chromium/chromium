// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_FRAME_QUEUE_TRANSFERRING_OPTIMIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_FRAME_QUEUE_TRANSFERRING_OPTIMIZER_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/breakout_box/frame_queue.h"
#include "third_party/blink/renderer/modules/breakout_box/frame_queue_underlying_source.h"
#include "third_party/blink/renderer/modules/breakout_box/transferred_frame_queue_underlying_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

template <typename NativeFrameType>
class FrameQueueTransferringOptimizer final
    : public ReadableStreamTransferringOptimizer {
 public:
  using FrameQueueHost = FrameQueueUnderlyingSource<NativeFrameType>;

  using ConnectHostCallback = CrossThreadOnceFunction<void(
      scoped_refptr<base::SequencedTaskRunner>,
      CrossThreadPersistent<
          TransferredFrameQueueUnderlyingSource<NativeFrameType>>,
      base::TimeTicks time_origin,
      bool is_cross_origin_isolated)>;

  FrameQueueTransferringOptimizer(
      FrameQueueHost*,
      scoped_refptr<base::SequencedTaskRunner> host_runner,
      scoped_refptr<FrameQueue<NativeFrameType>> frame_queue,
      std::string device_id,
      wtf_size_t frame_pool_size,
      std::optional<base::ThreadType> thread_type,
      ConnectHostCallback connect_host_callback);
  ~FrameQueueTransferringOptimizer() override = default;

  UnderlyingSourceBase* PerformInProcessOptimization(
      ScriptState* script_state) override;

 private:
  CrossThreadWeakPersistent<FrameQueueHost> host_;
  scoped_refptr<base::SequencedTaskRunner> host_runner_;
  scoped_refptr<FrameQueue<NativeFrameType>> frame_queue_;
  const std::string device_id_;
  const wtf_size_t frame_pool_size_;
  const std::optional<base::ThreadType> thread_type_;
  ConnectHostCallback connect_host_callback_;
};

extern template class MODULES_EXTERN_TEMPLATE_EXPORT
    FrameQueueTransferringOptimizer<scoped_refptr<media::VideoFrame>>;
extern template class MODULES_EXTERN_TEMPLATE_EXPORT
    FrameQueueTransferringOptimizer<scoped_refptr<media::AudioBuffer>>;

using VideoFrameQueueTransferOptimizer =
    FrameQueueTransferringOptimizer<scoped_refptr<media::VideoFrame>>;
using AudioDataQueueTransferOptimizer =
    FrameQueueTransferringOptimizer<scoped_refptr<media::AudioBuffer>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_FRAME_QUEUE_TRANSFERRING_OPTIMIZER_H_
