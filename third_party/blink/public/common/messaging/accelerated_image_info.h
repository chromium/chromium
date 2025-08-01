// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_ACCELERATED_IMAGE_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_ACCELERATED_IMAGE_INFO_H_

#include "base/functional/callback.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

// This struct represents all the information needed to create an
// AcceleratedStaticImageBitmap in the receiving process.
// See third_party/blink/public/mojom/messaging/static_bitmap_image.mojom
// for details.
struct BLINK_COMMON_EXPORT AcceleratedImageInfo {
  gpu::ExportedSharedImage shared_image;
  gpu::SyncToken sync_token;
  SkAlphaType alpha_type;
  base::OnceCallback<void(const gpu::SyncToken& sync_token)> release_callback;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_ACCELERATED_IMAGE_INFO_H_
