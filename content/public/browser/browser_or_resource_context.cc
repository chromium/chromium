// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_or_resource_context.h"

#include "base/check_deref.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/resource_context.h"

namespace content {

BrowserOrResourceContext::BrowserOrResourceContext() = default;

BrowserOrResourceContext::BrowserOrResourceContext(
    BrowserContext* browser_context)
    : storage_(raw_ref(CHECK_DEREF(browser_context))) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BrowserOrResourceContext::BrowserOrResourceContext(
    ResourceContext* resource_context)
    : storage_(raw_ref(CHECK_DEREF(resource_context))) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

BrowserOrResourceContext::~BrowserOrResourceContext() = default;
BrowserOrResourceContext::BrowserOrResourceContext(
    const BrowserOrResourceContext&) = default;
BrowserOrResourceContext& BrowserOrResourceContext::operator=(
    const BrowserOrResourceContext&) = default;

}  // namespace content
