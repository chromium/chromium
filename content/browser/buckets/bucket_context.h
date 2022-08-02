// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_
#define CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"
#include "url/origin.h"

namespace content {

// This class encapsulates logic and data relevant to a particular bucket. There
// is one created for each bucket that a renderer creates.
class CONTENT_EXPORT BucketContext {
 public:
  explicit BucketContext(const GlobalRenderFrameHostId& render_frame_host_id);
  explicit BucketContext(int render_process_id);
  BucketContext(const BucketContext& other);
  BucketContext operator=(const BucketContext& other) = delete;
  ~BucketContext();

  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission_type,
      const url::Origin& origin) const;

  void set_permission_status_for_test(blink::mojom::PermissionStatus status) {
    permission_status_for_test_ = status;
  }

 private:
  absl::variant<int, GlobalRenderFrameHostId> id_;
  absl::optional<blink::mojom::PermissionStatus> permission_status_for_test_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_
