// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_factory.h"

#include <memory>

#include "base/check.h"
#include "content/browser/renderer_host/page_impl.h"

namespace content {

// static
PageFactory* PageFactory::factory_ = nullptr;

// static
std::unique_ptr<PageImpl> PageFactory::Create(RenderFrameHostImpl& rfh,
                                              PageDelegate& delegate) {
  if (factory_) {
    return factory_->CreatePage(rfh, delegate);
  }

  return std::make_unique<PageImpl>(rfh, delegate);
}

// static
void PageFactory::RegisterFactory(PageFactory* factory) {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = factory;
}

// static
void PageFactory::UnregisterFactory() {
  DCHECK(factory_) << "No factory to unregister.";
  factory_ = nullptr;
}

}  // namespace content
