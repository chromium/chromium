// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_INTERFACE_BINDERS_H_
#define EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_INTERFACE_BINDERS_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class RenderFrameHost;
}

namespace mojo {
template <typename>
class BinderMapWithContext;
}  // namespace mojo

namespace extensions {

class Extension;

void PopulateExtensionFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_INTERFACE_BINDERS_H_
