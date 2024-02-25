// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"

#include <utility>

#include "content/public/browser/child_process_host.h"
#include "ipc/ipc_message.h"

namespace content {

WebContents::CreateParams::CreateParams(BrowserContext* context,
                                        base::Location creator_location)
    : CreateParams(context, nullptr, creator_location) {}

WebContents::CreateParams::CreateParams(BrowserContext* context,
                                        scoped_refptr<SiteInstance> site,
                                        base::Location creator_location)
    : browser_context(context),
      site_instance(std::move(site)),
      creator_location(creator_location) {}

WebContents::CreateParams::CreateParams(const CreateParams& other) = default;

WebContents::CreateParams::~CreateParams() = default;

WebContents::ScopedIgnoreInputEvents::~ScopedIgnoreInputEvents() = default;

WebContents::ScopedIgnoreInputEvents::ScopedIgnoreInputEvents(
    ScopedIgnoreInputEvents&& rhs) {
  on_destruction_cb_ = std::move(rhs.on_destruction_cb_);
}

WebContents::ScopedIgnoreInputEvents&
WebContents::ScopedIgnoreInputEvents::operator=(ScopedIgnoreInputEvents&& rhs) {
  on_destruction_cb_ = std::move(rhs.on_destruction_cb_);
  return *this;
}

WebContents::ScopedIgnoreInputEvents::ScopedIgnoreInputEvents(
    base::OnceClosure on_destruction_cb)
    : on_destruction_cb_(std::move(on_destruction_cb)) {}

}  // namespace content
