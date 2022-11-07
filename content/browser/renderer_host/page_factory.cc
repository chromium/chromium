// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_factory.h"

#include <memory>

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/site_instance_group.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

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
