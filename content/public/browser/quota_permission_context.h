// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_QUOTA_PERMISSION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_QUOTA_PERMISSION_CONTEXT_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/public/common/storage_quota_params.h"

namespace content {

class QuotaPermissionContext
    : public base::RefCountedThreadSafe<QuotaPermissionContext> {
 public:
  enum QuotaPermissionResponse {
    QUOTA_PERMISSION_RESPONSE_UNKNOWN,
    QUOTA_PERMISSION_RESPONSE_ALLOW,
    QUOTA_PERMISSION_RESPONSE_DISALLOW,
    QUOTA_PERMISSION_RESPONSE_CANCELLED,
  };

  using PermissionCallback = base::OnceCallback<void(QuotaPermissionResponse)>;

  virtual void RequestQuotaPermission(const StorageQuotaParams& params,
                                      int render_process_id,
                                      PermissionCallback callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<QuotaPermissionContext>;
  virtual ~QuotaPermissionContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_QUOTA_PERMISSION_CONTEXT_H_
