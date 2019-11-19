// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RESOURCE_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_RESOURCE_CONTEXT_IMPL_H_

#include "content/common/content_export.h"
#include "content/public/browser/resource_context.h"

namespace content {

class BrowserContext;
class URLDataManagerBackend;

// Getters for objects that are part of BrowserContext which are also used on
// the IO thread. These are only accessed by content so they're not on the
// public API.

URLDataManagerBackend* GetURLDataManagerForResourceContext(
    ResourceContext* context);

// Initialize the above data on the ResourceContext from a given BrowserContext.
CONTENT_EXPORT void InitializeResourceContext(BrowserContext* browser_context);

}  // namespace content

#endif  // CONTENT_BROWSER_RESOURCE_CONTEXT_IMPL_H_
