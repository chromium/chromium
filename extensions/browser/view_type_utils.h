// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VIEW_TYPE_UTILS_H_
#define EXTENSIONS_BROWSER_VIEW_TYPE_UTILS_H_

#include "extensions/common/mojom/view_type.mojom.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace extensions {

// Get the type of a WebContents.
// Use GetViewType(RenderFrameHost) where applicable.
// GetViewType handles a NULL `tab` for convenience by returning
// mojom::ViewType::kInvalid.
mojom::ViewType GetViewType(content::WebContents* tab);

// Return the closest view type of the associated frame tree. If `frame_host` is
// contained in a guest frame tree this will return
// mojom::ViewType::kExtensionGuest, otherwise it will return the type set
// for the associated WebContents.
mojom::ViewType GetViewType(content::RenderFrameHost* frame_host);

// Set the type of a WebContents.
void SetViewType(content::WebContents* tab, mojom::ViewType type);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_VIEW_TYPE_UTILS_H_
