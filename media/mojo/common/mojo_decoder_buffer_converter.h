// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_COMMON_MOJO_DECODER_BUFFER_CONVERTER_
#define MEDIA_MOJO_COMMON_MOJO_DECODER_BUFFER_CONVERTER_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace media {

class DecoderBuffer;

// Returns the default capacity to be used with MojoDecoderBufferReader and
// MojoDecoderBufferWriter for |type|.
uint32_t GetDefaultDecoderBufferConverterCapacity(DemuxerStream::Type type);

// Combines mojom::DecoderBuffers with data read from a DataPipe to produce
// media::DecoderBuffers (counterpart of MojoDecoderBufferWriter).
class MojoDecoderBufferReader {
 public:
  using ReadCB = base::OnceCallback<void(scoped_refptr<DecoderBuffer>)>;

  // Creates a MojoDecoderBufferReader of |capacity| bytes and set the
  // |producer_handle|.
  static std::unique_ptr<MojoDecoderBufferReader> Create(
      uint32_t capacity,
      mojo::ScopedDataPipeProducerHandle* producer_handle);

  // Hold the consumer handle to read DecoderBuffer data.
  explicit MojoDecoderBufferReader(
      mojo::ScopedDataPipeConsumerHandle consumer_handle);

  ~MojoDecoderBufferReader();

  // Enqueues conversion of and reading data for a mojom::DecoderBuffer. Once
  // the data has been read, |read_cb| will be called with the converted
  // media::DecoderBuffer.
  //
  // |read_cb| will be called in the same order as ReadDecoderBuffer(). This
  // order must match the order that the data was written into the DataPipe!
  // Callbacks may run on the original stack, on a Mojo stack, or on a future
  // ReadDecoderBuffer() stack.
  //
  // If reading fails (for example, if the DataPipe is closed), |read_cb| will
  // be called with nullptr.
  void ReadDecoderBuffer(mojom::DecoderBufferPtr buffer, ReadCB read_cb);

  // Reads all pending data from the pipe and fire all pending ReadCBs, after
  // which fire the |flush_cb|. No further ReadDecoderBuffer() or Flush() calls
  // should be made before |flush_cb| is fired.
  // Note that |flush_cb| may be called on the same call stack as this Flush()
  // call if there are no pending reads.
  void Flush(base::OnceClosure flush_cb);

  // Whether there's any pending reads in |this|.
  bool HasPendingReads() const;

 private:
  void CancelReadCB(ReadCB read_cb);
  void CancelAllPendingReadCBs();
  void CompleteCurrentRead();
  void ScheduleNextRead();
  void OnPipeReadable(MojoResult result, const mojo::HandleSignalsState& state);
  void ProcessPendingReads();
  void OnPipeError(MojoResult result);

  // Read side of the DataPipe for receiving DecoderBuffer data.
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  // Provides notification about |consumer_handle_| readiness.
  mojo::SimpleWatcher pipe_watcher_;
  bool armed_;

  // Buffers waiting to be read in sequence.
  base::circular_deque<scoped_refptr<DecoderBuffer>> pending_buffers_;

  // Callbacks for pending buffers.
  base::circular_deque<ReadCB> pending_read_cbs_;

  // Callback for Flush().
  base::OnceClosure flush_cb_;

  // Number of bytes already read into the current buffer.
  uint32_t bytes_read_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecoderBufferReader);
};

// Converts media::DecoderBuffers to mojom::DecoderBuffers, writing the data
// part to a DataPipe (counterpart of MojoDecoderBufferReader).
//
// If necessary, writes to the DataPipe will be chunked to fit.
// MojoDecoderBufferWriter maintains an internal queue of buffers to enable
// this asynchronous process.
//
// On DataPipe closure, future calls to WriteDecoderBuffer() will return
// nullptr. There is no mechanism to determine which past writes were
// successful prior to the closure.
class MojoDecoderBufferWriter {
 public:
  // Creates a MojoDecoderBufferWriter of |capacity| bytes and set the
  // |consumer_handle|.
  static std::unique_ptr<MojoDecoderBufferWriter> Create(
      uint32_t capacity,
      mojo::ScopedDataPipeConsumerHandle* consumer_handle);

  // Hold the producer handle to write DecoderBuffer data.
  explicit MojoDecoderBufferWriter(
      mojo::ScopedDataPipeProducerHandle producer_handle);

  ~MojoDecoderBufferWriter();

  // Converts a media::DecoderBuffer to a mojom::DecoderBuffer and enqueues the
  // data to be written to the DataPipe.
  //
  // Returns nullptr if the DataPipe is already closed.
  mojom::DecoderBufferPtr WriteDecoderBuffer(
      scoped_refptr<DecoderBuffer> media_buffer);

 private:
  void ScheduleNextWrite();
  void OnPipeWritable(MojoResult result, const mojo::HandleSignalsState& state);
  void ProcessPendingWrites();
  void OnPipeError(MojoResult result);

  // Write side of the DataPipe for sending DecoderBuffer data.
  mojo::ScopedDataPipeProducerHandle producer_handle_;

  // Provides notifications about |producer_handle_| readiness.
  mojo::SimpleWatcher pipe_watcher_;
  bool armed_;

  // Buffers waiting to be written in sequence.
  base::circular_deque<scoped_refptr<DecoderBuffer>> pending_buffers_;

  // Number of bytes already written from the current buffer.
  uint32_t bytes_written_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecoderBufferWriter);
};

}  // namespace media

#endif  // MEDIA_MOJO_COMMON_MOJO_DECODER_BUFFER_CONVERTER_
