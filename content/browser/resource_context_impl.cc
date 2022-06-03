// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_context_impl.h"

#include "content/public/browser/browser_context.h"

using base::UserDataAdapter;

namespace content {

ResourceContext::ResourceContext() {}

ResourceContext::~ResourceContext() {
}

void InitializeResourceContext(BrowserContext* browser_context) {
  ResourceContext* resource_context = browser_context->GetResourceContext();

  resource_context->DetachFromSequence();
}

}  // namespace content
