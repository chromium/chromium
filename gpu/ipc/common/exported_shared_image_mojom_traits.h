// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_EXPORTED_SHARED_IMAGE_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_EXPORTED_SHARED_IMAGE_MOJOM_TRAITS_H_

#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/exported_shared_image.mojom-shared.h"
#include "gpu/ipc/common/mailbox_mojom_traits.h"
#include "gpu/ipc/common/shared_image_metadata_mojom_traits.h"
#include "gpu/ipc/common/sync_token_mojom_traits.h"

namespace mojo {

template <>
struct GPU_EXPORT StructTraits<gpu::mojom::ExportedSharedImageDataView,
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

  static uint32_t texture_target(const gpu::ExportedSharedImage& shared_image) {
    return shared_image.texture_target_;
  }

  static bool Read(gpu::mojom::ExportedSharedImageDataView data,
                   gpu::ExportedSharedImage* out) {
    if (!data.ReadMailbox(&out->mailbox_) ||
        !data.ReadMetadata(&out->metadata_) ||
        !data.ReadCreationSyncToken(&out->creation_sync_token_)) {
      return false;
    }
    out->texture_target_ = data.texture_target();
    return true;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_EXPORTED_SHARED_IMAGE_MOJOM_TRAITS_H_
