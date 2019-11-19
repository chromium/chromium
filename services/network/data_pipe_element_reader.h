// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DATA_PIPE_ELEMENT_READER_H_
#define SERVICES_NETWORK_DATA_PIPE_ELEMENT_READER_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/completion_once_callback.h"
#include "net/base/upload_element_reader.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"

namespace net {
class IOBuffer;
}

namespace network {

// A subclass of net::UploadElementReader to read data pipes.
class COMPONENT_EXPORT(NETWORK_SERVICE) DataPipeElementReader
    : public net::UploadElementReader {
 public:
  // |resource_request_body| is just passed in to keep the object around for the
  // life of the ElementReader.
  //
  // TODO(mmenke): This class doesn't handle the case where the DataPipeGetter
  // pipe is closed. That should be fixed.
  DataPipeElementReader(
      scoped_refptr<ResourceRequestBody> resource_request_body,
      mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter);

  ~DataPipeElementReader() override;

  // net::UploadElementReader implementation:
  int Init(net::CompletionOnceCallback callback) override;
  uint64_t GetContentLength() const override;
  uint64_t BytesRemaining() const override;
  int Read(net::IOBuffer* buf,
           int buf_length,
           net::CompletionOnceCallback callback) override;

 private:
  // Callback invoked by DataPipeGetter::Read.
  void ReadCallback(int32_t status, uint64_t size);

  // Called by |handle_watcher_| when data is available or the pipe was closed,
  // and there's a pending Read() call.
  void OnHandleReadable(MojoResult result);

  // Attempts to read data from |data_pipe_| and write it to |buf|. On success,
  // writes the amount of data written. On failure, returns a net error code. If
  // no data was available yet, tells |handle_watcher_| to start watching the
  // pipe for data to become available and returns ERR_IO_PENDING. It's up to
  // the caller to update |buf_| and |buf_length_| if needed.
  int ReadInternal(net::IOBuffer* buf, int buf_length);

  scoped_refptr<ResourceRequestBody> resource_request_body_;
  mojo::Remote<mojom::DataPipeGetter> data_pipe_getter_;
  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  mojo::SimpleWatcher handle_watcher_;

  // Write buffer and its length. Populated when Read() is called but returns
  // ERR_IO_PENDING. Cleared once the read completes.
  scoped_refptr<net::IOBuffer> buf_;
  int buf_length_ = 0;

  // Total size of input, as passed to ReadCallback().
  uint64_t size_ = 0;

  uint64_t bytes_read_ = 0;
  net::CompletionOnceCallback init_callback_;
  net::CompletionOnceCallback read_callback_;

  base::WeakPtrFactory<DataPipeElementReader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DataPipeElementReader);
};

}  // namespace network

#endif  // SERVICES_NETWORK_DATA_PIPE_ELEMENT_READER_H_
