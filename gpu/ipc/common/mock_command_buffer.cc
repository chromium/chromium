// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/mock_command_buffer.h"

namespace gpu {

MockCommandBuffer::MockCommandBuffer() = default;

MockCommandBuffer::~MockCommandBuffer() = default;

void MockCommandBuffer::Bind(
    mojo::PendingAssociatedReceiver<mojom::CommandBuffer> receiver) {
  receiver_.Bind(std::move(receiver));
}

}  // namespace gpu
