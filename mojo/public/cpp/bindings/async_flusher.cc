// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/async_flusher.h"

#include <utility>

namespace mojo {

AsyncFlusher::AsyncFlusher() = default;

AsyncFlusher::AsyncFlusher(AsyncFlusher&&) = default;

AsyncFlusher& AsyncFlusher::operator=(AsyncFlusher&&) = default;

AsyncFlusher::~AsyncFlusher() = default;

void AsyncFlusher::SetPipe(ScopedMessagePipeHandle pipe) {
  pipe_ = std::move(pipe);
}

ScopedMessagePipeHandle AsyncFlusher::PassPipe() {
  return std::move(pipe_);
}

}  // namespace mojo
