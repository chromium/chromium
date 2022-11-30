// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_QUOTA_PERMISSION_CONTEXT_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_QUOTA_PERMISSION_CONTEXT_H_

#include "content/public/browser/quota_permission_context.h"

namespace headless {

class HeadlessQuotaPermissionContext : public content::QuotaPermissionContext {
 public:
  HeadlessQuotaPermissionContext();

  HeadlessQuotaPermissionContext(const HeadlessQuotaPermissionContext&) =
      delete;
  HeadlessQuotaPermissionContext& operator=(
      const HeadlessQuotaPermissionContext&) = delete;

  // The callback will be dispatched on the IO thread.
  void RequestQuotaPermission(const content::StorageQuotaParams& params,
                              int render_process_id,
                              PermissionCallback callback) override;

 private:
  ~HeadlessQuotaPermissionContext() override;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_QUOTA_PERMISSION_CONTEXT_H_
