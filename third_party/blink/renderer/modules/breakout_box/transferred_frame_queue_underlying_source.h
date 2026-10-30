// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_TRANSFERRED_FRAME_QUEUE_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_TRANSFERRED_FRAME_QUEUE_UNDERLYING_SOURCE_H_

#include <optional>

#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/modules/breakout_box/frame_queue_underlying_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

template <typename NativeFrameType>
class TransferredFrameQueueUnderlyingSource
    : public FrameQueueUnderlyingSource<NativeFrameType> {
 public:
  using FrameQueueHost = FrameQueueUnderlyingSource<NativeFrameType>;

  TransferredFrameQueueUnderlyingSource(
      ScriptState*,
      scoped_refptr<FrameQueue<NativeFrameType>> queue,
      scoped_refptr<base::SequencedTaskRunner> host_runner,
      CrossThreadWeakHandle<FrameQueueHost> host_handle,
      std::string device_id = std::string(),
      wtf_size_t frame_pool_size = 0,
      std::optional<base::ThreadType> thread_type = std::nullopt);
  ~TransferredFrameQueueUnderlyingSource() override = default;

  TransferredFrameQueueUnderlyingSource(
      const TransferredFrameQueueUnderlyingSource&) = delete;
  TransferredFrameQueueUnderlyingSource& operator=(
      const TransferredFrameQueueUnderlyingSource&) = delete;

  // FrameQueueUnderlyingSource<NativeFrameType> implementation.
  bool StartFrameDelivery() override;
  void StopFrameDelivery() override;
  void Close() override;

  // ExecutionLifecycleObserver
  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

 private:
  scoped_refptr<base::SequencedTaskRunner> host_runner_;
  // Weak handle because this object is constructed on the destination thread
  // from FrameQueueTransferringOptimizer's CrossThreadWeakHandle, and a strong
  // CrossThreadHandle can only be constructed on the host's creation thread.
  //
  // This doesn't shorten the host's lifetime while frames flow: the host keeps
  // itself alive while it is connected to its track, and it stays connected
  // until it is closed (see StartFrameDelivery() in
  // MediaStreamVideoTrackUnderlyingSource and
  // MediaStreamAudioTrackUnderlyingSource). Requests posted after the host is
  // gone are dropped, except the one from StartFrameDelivery(), which closes
  // this source instead so that the transferred stream doesn't hang.
  CrossThreadWeakHandle<FrameQueueHost> host_handle_;
};

extern template class MODULES_EXTERN_TEMPLATE_EXPORT
    TransferredFrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;
extern template class MODULES_EXTERN_TEMPLATE_EXPORT
    TransferredFrameQueueUnderlyingSource<scoped_refptr<media::AudioBuffer>>;

using TransferredVideoFrameQueueUnderlyingSource =
    TransferredFrameQueueUnderlyingSource<scoped_refptr<media::VideoFrame>>;
using TransferredAudioDataQueueUnderlyingSource =
    TransferredFrameQueueUnderlyingSource<scoped_refptr<media::AudioBuffer>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_TRANSFERRED_FRAME_QUEUE_UNDERLYING_SOURCE_H_
