// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_OPENSCREEN_DECODER_BUFFER_READER_H_
#define MEDIA_CAST_OPENSCREEN_DECODER_BUFFER_READER_H_

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/decoder_buffer.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media::cast {

// This class wraps functionality around reading a media::DecoderBuffer from
// a mojo pipe, while providing synchronization around the following three
// operations:
// - Providing the media::mojom::DecoderBufferPtr for a pending DecoderBuffer
//   read.
// - Reading the data for a DecoderBuffer from a |data_pipe| provided to the
//   ctor.
// - Requesting a new buffer callback by the embedding class.
class DecoderBufferReader {
 public:
  using NewBufferCb =
      base::RepeatingCallback<void(scoped_refptr<media::DecoderBuffer>)>;

  DecoderBufferReader(NewBufferCb new_buffer_cb,
                      mojo::ScopedDataPipeConsumerHandle data_pipe);
  DecoderBufferReader(DecoderBufferReader&& other,
                      mojo::ScopedDataPipeConsumerHandle data_pipe);
  ~DecoderBufferReader();

  // Queues up a DecodeBuffer to be read. The contents of this buffer will be
  // read from |decoder_buffer_reader_|.
  void ProvideBuffer(media::mojom::DecoderBufferPtr buffer);

  // Reads a single buffer when it is available, returning the data via the
  // Client.
  void ReadBufferAsync();

  // Informs this instance that an ongoing read call has been cancelled, and no
  // call to |new_buffer_cb_| is expected.
  void ClearReadPending();

  bool is_queue_empty() const { return pending_buffer_metadata_.empty(); }
  bool is_read_pending() const { return is_read_pending_; }

 private:
  void TryGetNextBuffer();
  void OnBufferReadFromDataPipe(scoped_refptr<media::DecoderBuffer> buffer);
  void CompletePendingRead();

  bool is_read_pending_ = false;

  NewBufferCb new_buffer_cb_;

  media::MojoDecoderBufferReader mojo_buffer_reader_;
  base::circular_deque<media::mojom::DecoderBufferPtr> pending_buffer_metadata_;
  scoped_refptr<media::DecoderBuffer> current_buffer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DecoderBufferReader> weak_factory_;
};

}  // namespace media::cast

#endif  // MEDIA_CAST_OPENSCREEN_DECODER_BUFFER_READER_H_
