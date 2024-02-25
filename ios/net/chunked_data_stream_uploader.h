// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CHUNKED_DATA_STREAM_UPLOADER_H_
#define IOS_NET_CHUNKED_DATA_STREAM_UPLOADER_H_

#include <stdint.h>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/upload_data_stream.h"

namespace net {
class IOBuffer;

// The ChunkedDataStreamUploader is used to support chunked data post for iOS
// NSMutableURLRequest HTTPBodyStream. Called on the network thread. It's
// responsible to coordinate the internal callbacks from network layer with the
// NSInputStream data. Rewind is not supported.
class ChunkedDataStreamUploader : public net::UploadDataStream {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Called when the request is ready to read the data for request body.
    // Data must be read in this function and put into |buf|. |buf_len| gives
    // the length of the provided buffer, and the return values gives the actual
    // bytes read. UploadDataStream::Read() currently does not support to return
    // failure, so need to handle the stream errors in the callback.
    virtual int OnRead(char* buffer, int buffer_length) = 0;
  };

  ChunkedDataStreamUploader(Delegate* delegate);

  ChunkedDataStreamUploader(const ChunkedDataStreamUploader&) = delete;
  ChunkedDataStreamUploader& operator=(const ChunkedDataStreamUploader&) =
      delete;

  ~ChunkedDataStreamUploader() override;

  // Interface for iOS layer to try to upload data. If there already has a
  // internal ReadInternal() callback ready from the network layer, data will be
  // writen to buffer immediately. Otherwise, it will do nothing in order to
  // wait internal callback. Once it is ready for the network layer to read
  // data, the OnRead() callback will be called.
  void UploadWhenReady(bool is_final_chunk);

  // The uploader interface for iOS layer to use.
  base::WeakPtr<ChunkedDataStreamUploader> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Internal function to implement data upload to network layer.
  int Upload();

  // net::UploadDataStream implementation:
  int InitInternal(const NetLogWithSource& net_log) override;
  int ReadInternal(IOBuffer* buffer, int buffer_length) override;
  void ResetInternal() override;

  const raw_ptr<Delegate> delegate_;

  // The pointer to the network layer buffer to send and the length of the
  // buffer.
  raw_ptr<net::IOBuffer> pending_read_buffer_;
  int pending_read_buffer_length_;

  // Flags indicating current upload process has network read callback pending.
  bool pending_internal_read_;

  // Flags indicating if current block is the last.
  bool is_final_chunk_;

  // Set to false when a read starts, does not support rewinding
  // for stream upload.
  bool is_front_of_stream_;

  base::WeakPtrFactory<ChunkedDataStreamUploader> weak_factory_;
};

}  // namespace net

#endif  // IOS_NET_CHUNKED_DATA_STREAM_UPLOADER_H_
