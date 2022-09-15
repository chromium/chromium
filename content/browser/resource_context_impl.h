// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RESOURCE_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_RESOURCE_CONTEXT_IMPL_H_

#include "content/public/browser/resource_context.h"

namespace content {

class BrowserContext;

// Getters for objects that are part of BrowserContext which are also used on
// the IO thread. These are only accessed by content so they're not on the
// public API.

// Initialize the above data on the ResourceContext from a given BrowserContext.
void InitializeResourceContext(BrowserContext* browser_context);

}  // namespace content

#endif  // CONTENT_BROWSER_RESOURCE_CONTEXT_IMPL_H_
