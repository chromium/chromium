// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/pending_flush.h"

#include <utility>

#include "mojo/public/cpp/bindings/async_flusher.h"

namespace mojo {

PendingFlush::PendingFlush(AsyncFlusher* flusher) {
  ScopedMessagePipeHandle flusher_pipe;
  CreateMessagePipe(/*options=*/nullptr, &pipe_, &flusher_pipe);
  flusher->SetPipe(std::move(flusher_pipe));
}

PendingFlush::PendingFlush(PendingFlush&& other) = default;

PendingFlush& PendingFlush::operator=(PendingFlush&& other) = default;

PendingFlush::~PendingFlush() = default;

ScopedMessagePipeHandle PendingFlush::PassPipe() {
  DCHECK(pipe_) << "This PendingFlush has already been consumed.";
  return std::move(pipe_);
}

}  // namespace mojo
