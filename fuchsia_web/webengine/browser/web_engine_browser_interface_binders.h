// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_BROWSER_INTERFACE_BINDERS_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_BROWSER_INTERFACE_BINDERS_H_

#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// PopulateFuchsiaFrameBinders() registers BrowserInterfaceBroker's
// GetInterface() handler callbacks for Fuchsia-specific RenferFrame-scoped
// interfaces. This mechanism will replace interface registries and binders used
// for handling InterfaceProvider's GetInterface() calls (see crbug.com/718652).
void PopulateFuchsiaFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map);

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_BROWSER_INTERFACE_BINDERS_H_
