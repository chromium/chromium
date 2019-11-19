// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_SYNC_TOKEN_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_SYNC_TOKEN_MOJOM_TRAITS_H_

#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/sync_token.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::SyncTokenDataView, gpu::SyncToken> {
  static bool verified_flush(const gpu::SyncToken& token) {
    DCHECK(!token.HasData() || token.verified_flush());
    return token.verified_flush();
  }

  static gpu::mojom::CommandBufferNamespace namespace_id(
      const gpu::SyncToken& token) {
    return static_cast<gpu::mojom::CommandBufferNamespace>(
        token.namespace_id());
  }

  static uint64_t command_buffer_id(const gpu::SyncToken& token) {
    return token.command_buffer_id().GetUnsafeValue();
  }

  static uint64_t release_count(const gpu::SyncToken& token) {
    return token.release_count();
  }

  static bool Read(gpu::mojom::SyncTokenDataView data, gpu::SyncToken* out) {
    *out = gpu::SyncToken(
        static_cast<gpu::CommandBufferNamespace>(data.namespace_id()),
        gpu::CommandBufferId::FromUnsafeValue(data.command_buffer_id()),
        data.release_count());
    if (out->HasData()) {
      if (!data.verified_flush())
        return false;
      out->SetVerifyFlush();
    }
    return true;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_SYNC_TOKEN_MOJOM_TRAITS_H_
