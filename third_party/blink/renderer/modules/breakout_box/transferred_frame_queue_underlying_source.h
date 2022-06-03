// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_TRANSFERRED_FRAME_QUEUE_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_TRANSFERRED_FRAME_QUEUE_UNDERLYING_SOURCE_H_

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
      FrameQueueHost*,
      scoped_refptr<base::SequencedTaskRunner> host_runner);
  ~TransferredFrameQueueUnderlyingSource() override = default;

  TransferredFrameQueueUnderlyingSource(
      const TransferredFrameQueueUnderlyingSource&) = delete;
  TransferredFrameQueueUnderlyingSource& operator=(
      const TransferredFrameQueueUnderlyingSource&) = delete;

  // FrameQueueUnderlyingSource<NativeFrameType> implementation.
  bool StartFrameDelivery() override;
  void StopFrameDelivery() override;

  void Trace(Visitor*) const override;

 private:
  scoped_refptr<base::SequencedTaskRunner> host_runner_;
  CrossThreadPersistent<FrameQueueHost> host_;
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
