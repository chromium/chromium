// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CHUNKED_UPLOAD_DATA_STREAM_H_
#define NET_BASE_CHUNKED_UPLOAD_DATA_STREAM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/base/upload_data_stream.h"

namespace net {

class IOBuffer;

// Class with a push-based interface for uploading data. Buffers all data until
// the request is completed. Not recommended for uploading large amounts of
// seekable data, due to this buffering behavior.
class NET_EXPORT ChunkedUploadDataStream : public UploadDataStream {
 public:
  // Utility class that allows writing data to a particular
  // ChunkedUploadDataStream. It can outlive the associated
  // ChunkedUploadDataStream, and the URLRequest it is associated with, and
  // still be safely used. This allows the consumer to not have to worry about
  // the lifetime of the ChunkedUploadDataStream, which the owning URLRequest
  // may delete without warning.
  //
  // The writer may only be used on the ChunkedUploadDataStream's thread.
  class NET_EXPORT Writer {
   public:
    ~Writer();

    // Adds data to the stream. |is_done| should be true if this is the last
    // data to be appended. |data_len| must not be 0 unless |is_done| is true.
    // Once called with |is_done| being true, must never be called again.
    // Returns true if write was passed successfully on to the next layer,
    // though the data may not actually have been written to the underlying
    // URLRequest.  Returns false if unable to write the data failed because the
    // underlying ChunkedUploadDataStream was destroyed.
    bool AppendData(const char* data, int data_len, bool is_done);

   private:
    friend class ChunkedUploadDataStream;

    explicit Writer(base::WeakPtr<ChunkedUploadDataStream> upload_data_stream);

    const base::WeakPtr<ChunkedUploadDataStream> upload_data_stream_;

    DISALLOW_COPY_AND_ASSIGN(Writer);
  };

  explicit ChunkedUploadDataStream(int64_t identifier);

  ~ChunkedUploadDataStream() override;

  // Creates a Writer for appending data to |this|.  It's generally expected
  // that only one writer is created per stream, though multiple writers are
  // allowed.  All writers write to the same stream, and once one of them
  // appends data with |is_done| being true, no other writers may be used to
  // append data.
  std::unique_ptr<Writer> CreateWriter();

  // Adds data to the stream. |is_done| should be true if this is the last
  // data to be appended. |data_len| must not be 0 unless |is_done| is true.
  // Once called with |is_done| being true, must never be called again.
  // TODO(mmenke):  Consider using IOBuffers instead, to reduce data copies.
  // TODO(mmenke):  Consider making private, and having all consumers use
  //     Writers.
  void AppendData(const char* data, int data_len, bool is_done);

 private:
  // UploadDataStream implementation.
  int InitInternal(const NetLogWithSource& net_log) override;
  int ReadInternal(IOBuffer* buf, int buf_len) override;
  void ResetInternal() override;

  int ReadChunk(IOBuffer* buf, int buf_len);

  // Index and offset of next element of |upload_data_| to be read.
  size_t read_index_;
  size_t read_offset_;

  // True once all data has been appended to the stream.
  bool all_data_appended_;

  std::vector<std::unique_ptr<std::vector<char>>> upload_data_;

  // Buffer to write the next read's data to. Only set when a call to
  // ReadInternal reads no data.
  scoped_refptr<IOBuffer> read_buffer_;
  int read_buffer_len_;

  base::WeakPtrFactory<ChunkedUploadDataStream> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChunkedUploadDataStream);
};

}  // namespace net

#endif  // NET_BASE_CHUNKED_UPLOAD_DATA_STREAM_H_
