// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_PIPE_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_PIPE_H_

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

#include <memory>

namespace content {

class DevToolsStreamPipe : public DevToolsIOContext::Stream {
 public:
  static scoped_refptr<DevToolsStreamPipe> Create(
      DevToolsIOContext* context,
      mojo::ScopedDataPipeConsumerHandle pipe,
      bool is_binary);
  const std::string& handle() const { return handle_; }

 private:
  struct ReadRequest;

  DevToolsStreamPipe(DevToolsIOContext* context,
                     mojo::ScopedDataPipeConsumerHandle pipe,
                     bool is_binary);
  ~DevToolsStreamPipe() override;

  bool SupportsSeek() const override;
  void Read(off_t position, size_t max_size, ReadCallback callback) override;

  void OnPipeSignalled(MojoResult result,
                       const mojo::HandleSignalsState& state);
  void DispatchResponse();
  void DispatchEOFOrError(bool is_eof);

  const std::string handle_;
  const mojo::ScopedDataPipeConsumerHandle pipe_;
  const bool is_binary_;

  mojo::SimpleWatcher pipe_watcher_;
  base::queue<ReadRequest> read_requests_;
  std::string buffer_;
  Status last_status_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_STREAM_PIPE_H_
