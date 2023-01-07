// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_STORAGE_QUOTA_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_STORAGE_QUOTA_PARAMS_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"

namespace content {

// Parameters from the renderer to the browser process on a
// RequestStorageQuota call.
struct CONTENT_EXPORT StorageQuotaParams {
  StorageQuotaParams()
      : render_frame_id(MSG_ROUTING_NONE),
        storage_type(blink::mojom::StorageType::kTemporary),
        requested_size(0) {}

  int render_frame_id;
  // TODO(sashab): Change this to url::Origin, crbug.com/598424.
  GURL origin_url;
  blink::mojom::StorageType storage_type;
  uint64_t requested_size;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_STORAGE_QUOTA_PARAMS_H_
