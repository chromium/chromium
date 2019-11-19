// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/isolation_context.h"

namespace content {

IsolationContext::IsolationContext(BrowserContext* browser_context)
    : browser_or_resource_context_(BrowserOrResourceContext(browser_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

IsolationContext::IsolationContext(BrowsingInstanceId browsing_instance_id,
                                   BrowserContext* browser_context)
    : IsolationContext(browsing_instance_id,
                       BrowserOrResourceContext(browser_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

IsolationContext::IsolationContext(
    BrowsingInstanceId browsing_instance_id,
    BrowserOrResourceContext browser_or_resource_context)
    : browsing_instance_id_(browsing_instance_id),
      browser_or_resource_context_(browser_or_resource_context) {}

}  // namespace content
