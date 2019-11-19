// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_running_info.h"

namespace content {

ServiceWorkerRunningInfo::ServiceWorkerRunningInfo(const GURL& script_url,
                                                   const GURL& scope,
                                                   int64_t render_process_id)
    : script_url(script_url),
      scope(scope),
      render_process_id(render_process_id) {}

ServiceWorkerRunningInfo::ServiceWorkerRunningInfo(
    ServiceWorkerRunningInfo&& other) noexcept = default;
ServiceWorkerRunningInfo& ServiceWorkerRunningInfo::operator=(
    ServiceWorkerRunningInfo&& other) noexcept = default;

ServiceWorkerRunningInfo::~ServiceWorkerRunningInfo() = default;

}  // namespace content
