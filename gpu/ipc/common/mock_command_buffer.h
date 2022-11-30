// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_MOCK_COMMAND_BUFFER_H_
#define GPU_IPC_COMMON_MOCK_COMMAND_BUFFER_H_

#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

class MockCommandBuffer : public mojom::CommandBuffer {
 public:
  MockCommandBuffer();
  ~MockCommandBuffer() override;

  void Bind(mojo::PendingAssociatedReceiver<mojom::CommandBuffer> receiver);

  // mojom::CommandBuffer:
  MOCK_METHOD1(SetGetBuffer, void(int32_t));

 private:
  mojo::AssociatedReceiver<mojom::CommandBuffer> receiver_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_MOCK_COMMAND_BUFFER_H_
