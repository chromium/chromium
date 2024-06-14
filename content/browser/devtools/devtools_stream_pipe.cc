// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_stream_pipe.h"

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace content {

struct DevToolsStreamPipe::ReadRequest {
  ReadRequest() = delete;
  ReadRequest(uint32_t max_size, ReadCallback read_callback)
      : max_size(max_size), read_callback(std::move(read_callback)) {}

  uint32_t max_size;
  ReadCallback read_callback;
};

// static
scoped_refptr<DevToolsStreamPipe> DevToolsStreamPipe::Create(
    DevToolsIOContext* context,
    mojo::ScopedDataPipeConsumerHandle pipe,
    bool is_binary) {
  return new DevToolsStreamPipe(context, std::move(pipe), is_binary);
}

DevToolsStreamPipe::DevToolsStreamPipe(DevToolsIOContext* context,
                                       mojo::ScopedDataPipeConsumerHandle pipe,
                                       bool is_binary)
    : DevToolsIOContext::Stream(base::SequencedTaskRunner::GetCurrentDefault()),
      handle_(Register(context)),
      pipe_(std::move(pipe)),
      is_binary_(is_binary),
      pipe_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      last_status_(StatusSuccess) {
  MojoResult res = pipe_watcher_.Watch(
      pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&DevToolsStreamPipe::OnPipeSignalled,
                          base::Unretained(this)));
  DCHECK_EQ(MOJO_RESULT_OK, res);
}

DevToolsStreamPipe::~DevToolsStreamPipe() = default;

bool DevToolsStreamPipe::SupportsSeek() const {
  return false;
}

void DevToolsStreamPipe::Read(off_t position,
                              size_t max_size,
                              ReadCallback callback) {
  DCHECK(position == -1);
  if (last_status_ != StatusSuccess) {
    DCHECK(read_requests_.empty());
    std::move(callback).Run(std::make_unique<std::string>(), false,
                            last_status_);
    return;
  }
  read_requests_.emplace(max_size, std::move(callback));
  if (read_requests_.size() == 1lu)
    pipe_watcher_.ArmOrNotify();
}

void DevToolsStreamPipe::OnPipeSignalled(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK_EQ(StatusSuccess, last_status_);
  DCHECK(!read_requests_.empty());

  if (result != MOJO_RESULT_OK) {
    DispatchEOFOrError(state.peer_closed());
    return;
  }
  while (!read_requests_.empty()) {
    base::span<const uint8_t> pipe_bytes;
    MojoResult res = pipe_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, pipe_bytes);
    if (res == MOJO_RESULT_FAILED_PRECONDITION) {
      DCHECK(state.peer_closed());
      DispatchEOFOrError(state.peer_closed());
      return;
    }
    DCHECK_EQ(MOJO_RESULT_OK, res);
    auto& request = read_requests_.front();
    const size_t bytes_to_read =
        std::min(pipe_bytes.size(), size_t{request.max_size} - buffer_.size());
    // Dispatch available bytes (but no more than requested), when there are
    // multiple requests pending. If we just have a single read request, it's
    // more efficient (and easier for client) to only dispatch when enough bytes
    // are available or eof has been reached.
    const bool fulfill_entire_request = read_requests_.size() == 1ul;
    if (fulfill_entire_request)
      buffer_.reserve(request.max_size);
    buffer_.append(base::as_string_view(pipe_bytes.first(bytes_to_read)));
    pipe_->EndReadData(bytes_to_read);
    DCHECK_LE(buffer_.size(), request.max_size);
    if (buffer_.size() < request.max_size && fulfill_entire_request)
      break;
    DispatchResponse();
    if (bytes_to_read == pipe_bytes.size()) {
      break;
    }
  }
  if (!read_requests_.empty())
    pipe_watcher_.ArmOrNotify();
}

void DevToolsStreamPipe::DispatchResponse() {
  auto data = std::make_unique<std::string>(std::move(buffer_));
  if (is_binary_ && !data->empty())
    *data.get() = base::Base64Encode(*data);
  std::move(read_requests_.front().read_callback)
      .Run(std::move(data), is_binary_, last_status_);
  read_requests_.pop();
}

void DevToolsStreamPipe::DispatchEOFOrError(bool is_eof) {
  // For consistency with other implementation, do not report EOF or failure
  // condition along with actual data, do it for the next request instead.
  if (!buffer_.empty())
    DispatchResponse();
  last_status_ = is_eof ? StatusEOF : StatusFailure;

  while (!read_requests_.empty())
    DispatchResponse();
}

}  // namespace content
