// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_EXPORTED_SHARED_IMAGE_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_EXPORTED_SHARED_IMAGE_MOJOM_TRAITS_H_

#include <optional>

#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/exported_shared_image.mojom-shared.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/shared_image_metadata_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"

namespace mojo {

template <>
struct GPU_IPC_COMMON_EXPORT StructTraits<
    gpu::mojom::SharedImageExportResultDataView,
    gpu::SharedImageExportResult> {
  static const std::vector<gpu::SyncToken>& sync_tokens(
      const gpu::SharedImageExportResult& shared_image_exported_result) {
    return shared_image_exported_result.sync_tokens_;
  }

  static bool Read(gpu::mojom::SharedImageExportResultDataView data,
                   gpu::SharedImageExportResult* out) {
    if (!data.ReadSyncTokens(&out->sync_tokens_)) {
      return false;
    }
    return true;
  }
};

template <>
struct GPU_IPC_COMMON_EXPORT StructTraits<
    gpu::mojom::ExportedSharedImageDataView,
    gpu::ExportedSharedImage> {
  static const gpu::Mailbox& mailbox(const gpu::ExportedSharedImage& holder) {
    return holder.mailbox_;
  }

  static const gpu::SharedImageMetadata& metadata(
      const gpu::ExportedSharedImage& shared_image) {
    return shared_image.metadata_;
  }

  static const gpu::SyncToken& creation_sync_token(
      const gpu::ExportedSharedImage& shared_image) {
    return shared_image.creation_sync_token_;
  }

  static const std::vector<gpu::SyncToken>& managed_sync_tokens(
      const gpu::ExportedSharedImage& shared_image) {
    return shared_image.managed_sync_tokens_;
  }

  static const std::string& debug_label(
      const gpu::ExportedSharedImage& shared_image) {
    return shared_image.debug_label_;
  }

  static uint32_t texture_target(const gpu::ExportedSharedImage& shared_image) {
    return shared_image.texture_target_;
  }

  static bool is_software(const gpu::ExportedSharedImage& shared_image) {
    return shared_image.is_software_;
  }

  static std::optional<gfx::GpuMemoryBufferHandle>& buffer_handle(
      gpu::ExportedSharedImage& shared_image) {
    return shared_image.buffer_handle_;
  }

  static std::optional<gfx::BufferUsage>& buffer_usage(
      gpu::ExportedSharedImage& shared_image) {
    return shared_image.buffer_usage_;
  }

  static bool Read(gpu::mojom::ExportedSharedImageDataView data,
                   gpu::ExportedSharedImage* out) {
    if (!data.ReadMailbox(&out->mailbox_) || out->mailbox_.IsZero() ||
        !data.ReadMetadata(&out->metadata_) ||
        !data.ReadDebugLabel(&out->debug_label_) ||
        !data.ReadCreationSyncToken(&out->creation_sync_token_) ||
        !data.ReadManagedSyncTokens(&out->managed_sync_tokens_) ||
        !data.ReadBufferHandle(&out->buffer_handle_) ||
        !data.ReadBufferUsage(&out->buffer_usage_)) {
      return false;
    }
    // There must be at most one SyncToken per client sequence. Reject any
    // deserialized payload with duplicate client IDs to prevent receiving
    // processes from crashing or entering an inconsistent state.
    const auto& managed_tokens = out->managed_sync_tokens_;
    for (size_t i = 0; i < managed_tokens.size(); ++i) {
      for (size_t j = i + 1; j < managed_tokens.size(); ++j) {
        if (managed_tokens[i].GetClientId() ==
            managed_tokens[j].GetClientId()) {
          return false;
        }
      }
    }
    // If GpuMemoryBufferHandle is passed in, BufferUsage should also be passed.
    if (out->buffer_handle_ && !out->buffer_usage_) {
      return false;
    }
    if (out->buffer_handle_ && out->buffer_handle_->type == gfx::EMPTY_BUFFER) {
      return false;
    }
    out->texture_target_ = data.texture_target();
#if !BUILDFLAG(IS_FUCHSIA)
    // On Fuchsia, it is possible for texture_target_ to be 0 because Fuchsia
    // does not support import of external images to GL for usage with external
    // sampling. For other platforms, texture_target_ should always be non-zero.
    if (out->texture_target_ == 0) {
      return false;
    }
#endif
    out->is_software_ = data.is_software();
    return true;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_EXPORTED_SHARED_IMAGE_MOJOM_TRAITS_H_
