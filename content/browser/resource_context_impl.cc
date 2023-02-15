// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_context_impl.h"

#include "content/public/browser/browser_context.h"

using base::UserDataAdapter;

namespace content {

ResourceContext::ResourceContext() {}

ResourceContext::~ResourceContext() {
}

base::WeakPtr<ResourceContext> ResourceContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void InitializeResourceContext(BrowserContext* browser_context) {
  ResourceContext* resource_context = browser_context->GetResourceContext();

  resource_context->DetachFromSequence();
}

}  // namespace content
