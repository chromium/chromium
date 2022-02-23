// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/isolation_context.h"

namespace content {

IsolationContext::IsolationContext(BrowserContext* browser_context)
    : browser_or_resource_context_(BrowserOrResourceContext(browser_context)),
      is_guest_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

IsolationContext::IsolationContext(BrowsingInstanceId browsing_instance_id,
                                   BrowserContext* browser_context,
                                   bool is_guest)
    : IsolationContext(browsing_instance_id,
                       BrowserOrResourceContext(browser_context),
                       is_guest) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

IsolationContext::IsolationContext(
    BrowsingInstanceId browsing_instance_id,
    BrowserOrResourceContext browser_or_resource_context,
    bool is_guest)
    : browsing_instance_id_(browsing_instance_id),
      browser_or_resource_context_(browser_or_resource_context),
      is_guest_(is_guest) {}

}  // namespace content
