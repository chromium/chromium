// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DATA_PIPE_TO_SOURCE_STREAM_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DATA_PIPE_TO_SOURCE_STREAM_H_

#include "base/component_export.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/completion_once_callback.h"
#include "net/filter/source_stream.h"

namespace network {

class COMPONENT_EXPORT(NETWORK_CPP) DataPipeToSourceStream final
    : public net::SourceStream {
 public:
  explicit DataPipeToSourceStream(mojo::ScopedDataPipeConsumerHandle body);

  DataPipeToSourceStream(const DataPipeToSourceStream&) = delete;
  DataPipeToSourceStream& operator=(const DataPipeToSourceStream&) = delete;

  ~DataPipeToSourceStream() override;

  // net::SourceStream implementation.
  int Read(net::IOBuffer* buf,
           int buf_size,
           net::CompletionOnceCallback callback) override;
  std::string Description() const override;
  bool MayHaveMoreBytes() const override;

 private:
  void OnReadable(MojoResult result);
  void FinishReading();

  mojo::ScopedDataPipeConsumerHandle body_;
  mojo::SimpleWatcher handle_watcher_;

  bool inside_read_ = false;
  bool complete_ = false;

  scoped_refptr<net::IOBuffer> output_buf_;
  int output_buf_size_ = 0;
  net::CompletionOnceCallback pending_callback_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DATA_PIPE_TO_SOURCE_STREAM_H_
