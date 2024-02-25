// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_SERIALIZER_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_SERIALIZER_H_

#include <dawn/wire/WireClient.h>

#include <atomic>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"

namespace gpu {

class DecoderClient;

namespace webgpu {

class DawnServiceSerializer : public dawn::wire::CommandSerializer {
 public:
  explicit DawnServiceSerializer(DecoderClient* client);
  ~DawnServiceSerializer() override;
  size_t GetMaximumAllocationSize() const final;
  void* GetCmdSpace(size_t size) final;
  bool Flush() final;
  bool NeedsFlush() const;

 private:
  void FlushInternal();

  base::Lock lock_;
  raw_ptr<DecoderClient, DanglingUntriaged> client_;
  std::vector<uint8_t> buffer_;
  std::atomic<size_t> put_offset_;
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_SERVICE_SERIALIZER_H_
