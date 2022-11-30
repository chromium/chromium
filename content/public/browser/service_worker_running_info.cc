// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_worker_running_info.h"

namespace content {

ServiceWorkerRunningInfo::ServiceWorkerRunningInfo(
    const GURL& script_url,
    const GURL& scope,
    const blink::StorageKey& key,
    int64_t render_process_id,
    const blink::ServiceWorkerToken& token)
    : script_url(script_url),
      scope(scope),
      key(key),
      render_process_id(render_process_id),
      token(token) {}

ServiceWorkerRunningInfo::ServiceWorkerRunningInfo(
    ServiceWorkerRunningInfo&& other) noexcept = default;
ServiceWorkerRunningInfo& ServiceWorkerRunningInfo::operator=(
    ServiceWorkerRunningInfo&& other) noexcept = default;

ServiceWorkerRunningInfo::~ServiceWorkerRunningInfo() = default;

}  // namespace content
