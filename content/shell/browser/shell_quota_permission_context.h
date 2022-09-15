// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_QUOTA_PERMISSION_CONTEXT_H_
#define CONTENT_SHELL_BROWSER_SHELL_QUOTA_PERMISSION_CONTEXT_H_

#include "content/public/browser/quota_permission_context.h"

namespace content {

class ShellQuotaPermissionContext : public QuotaPermissionContext {
 public:
  ShellQuotaPermissionContext();

  ShellQuotaPermissionContext(const ShellQuotaPermissionContext&) = delete;
  ShellQuotaPermissionContext& operator=(const ShellQuotaPermissionContext&) =
      delete;

  // The callback will be dispatched on the IO thread.
  void RequestQuotaPermission(const StorageQuotaParams& params,
                              int render_process_id,
                              PermissionCallback callback) override;

 private:
  ~ShellQuotaPermissionContext() override;
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_QUOTA_PERMISSION_CONTEXT_H_
