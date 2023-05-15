// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/isolation_context.h"

namespace content {

IsolationContext::IsolationContext(BrowserContext* browser_context)
    : browser_or_resource_context_(BrowserOrResourceContext(browser_context)),
      is_guest_(false),
      is_fenced_(false),
      default_isolation_state_(
          OriginAgentClusterIsolationState::CreateForDefaultIsolation(
              browser_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

IsolationContext::IsolationContext(
    BrowsingInstanceId browsing_instance_id,
    BrowserContext* browser_context,
    bool is_guest,
    bool is_fenced,
    OriginAgentClusterIsolationState default_isolation_state)
    : IsolationContext(browsing_instance_id,
                       BrowserOrResourceContext(browser_context),
                       is_guest,
                       is_fenced,
                       default_isolation_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

IsolationContext::IsolationContext(
    BrowsingInstanceId browsing_instance_id,
    BrowserOrResourceContext browser_or_resource_context,
    bool is_guest,
    bool is_fenced,
    OriginAgentClusterIsolationState default_isolation_state)
    : browsing_instance_id_(browsing_instance_id),
      browser_or_resource_context_(browser_or_resource_context),
      is_guest_(is_guest),
      is_fenced_(is_fenced),
      default_isolation_state_(default_isolation_state) {}

}  // namespace content
