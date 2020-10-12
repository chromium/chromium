// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_CAST_STREAMING_STREAM_CONSUMER_H_
#define FUCHSIA_CAST_STREAMING_STREAM_CONSUMER_H_

#include <fuchsia/media/cpp/fidl.h>

#include "base/callback.h"
#include "base/timer/timer.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/openscreen/src/cast/streaming/receiver.h"
#include "third_party/openscreen/src/cast/streaming/receiver_session.h"

namespace cast_streaming {

// Attaches to an Open Screen Receiver to receive buffers of encoded data and
// invokes |frame_received_cb_| with each buffer.
//
// Internally, this class writes buffers of encoded data directly to
// |data_pipe_| rather than using a helper class like MojoDecoderBufferWriter.
// This allows us to use |data_pipe_| as an end-to-end buffer to cap memory
// usage. Receiving new buffers is delayed until the pipe has free memory again.
// The Open Screen library takes care of discarding buffers that are too old and
// requesting new key frames as needed.
class StreamConsumer : public openscreen::cast::Receiver::Consumer {
 public:
  using FrameReceivedCB =
      base::RepeatingCallback<void(media::mojom::DecoderBufferPtr)>;

  // |receiver| sends frames to this object. It must outlive this object.
  // |frame_received_cb| is called on every new frame, after a new frame has
  // been written to |data_pipe|. On error, |data_pipe| will be closed.
  // If no data is received for 10 seconds, |on_timeout| will be closed.
  StreamConsumer(openscreen::cast::Receiver* receiver,
                 mojo::ScopedDataPipeProducerHandle data_pipe,
                 FrameReceivedCB frame_received_cb,
                 base::OnceClosure on_timeout);
  ~StreamConsumer() final;

  StreamConsumer(const StreamConsumer&) = delete;
  StreamConsumer& operator=(const StreamConsumer&) = delete;

 private:
  // Maximum frame size that OnFramesReady() can accept.
  static constexpr uint32_t kMaxFrameSize = 512 * 1024;

  // Closes |data_pipe_| and resets the Consumer in |receiver_|. No frames will
  // be received after this call.
  void CloseDataPipeOnError();

  // Callback when |data_pipe_| can be written to again after it was full.
  void OnPipeWritable(MojoResult result);

  // openscreen::cast::Receiver::Consumer implementation.
  void OnFramesReady(int next_frame_buffer_size) final;

  openscreen::cast::Receiver* const receiver_;
  mojo::ScopedDataPipeProducerHandle data_pipe_;
  const FrameReceivedCB frame_received_cb_;

  // Provides notifications about |data_pipe_| readiness.
  mojo::SimpleWatcher pipe_watcher_;

  // Buffer used when |data_pipe_| is too full to accept the next frame size.
  uint8_t pending_buffer_[kMaxFrameSize];

  // Current offset for data |pending_buffer_| to be written to |data_pipe_|.
  size_t pending_buffer_offset_ = 0;

  // Remaining bytes to write from |pending_buffer_| to |data_pipe_|.
  size_t pending_buffer_remaining_bytes_ = 0;

  // Timer to trigger connection closure if no data is received for 10 seconds.
  base::OneShotTimer data_timeout_timer_;
};

}  // namespace cast_streaming

#endif  // FUCHSIA_CAST_STREAMING_STREAM_CONSUMER_H_
