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

class StoragePartitionImpl;

// This class encapsulates logic and data relevant to a particular bucket. There
// is one created for each bucket that a renderer creates.
class CONTENT_EXPORT BucketContext {
 public:
  BucketContext(const GlobalRenderFrameHostId& render_frame_host_id,
                const url::Origin& origin);
  BucketContext(int render_process_id, const url::Origin& origin);
  BucketContext(const BucketContext& other);
  BucketContext operator=(const BucketContext& other) = delete;
  ~BucketContext();

  const url::Origin& origin() const { return origin_; }
  StoragePartitionImpl* GetStoragePartition() const;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission_type) const;

  void set_permission_status_for_test(blink::mojom::PermissionStatus status) {
    permission_status_for_test_ = status;
  }

 private:
  absl::variant<int, GlobalRenderFrameHostId> id_;
  url::Origin origin_;
  absl::optional<blink::mojom::PermissionStatus> permission_status_for_test_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BUCKETS_BUCKET_CONTEXT_H_
