// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_browser_interface_binders.h"

#include <string>

#include "base/functional/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/mojo/keep_alive_impl.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/mojom/keep_alive.mojom.h"  // nogncheck

namespace extensions {

void PopulateExtensionFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK(extension);

  auto* context = render_frame_host->GetProcess()->GetBrowserContext();
  binder_map->Add<KeepAlive>(base::BindRepeating(
      &KeepAliveImpl::Create, context, base::RetainedRef(extension)));
}

}  // namespace extensions
