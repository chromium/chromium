// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VIEW_TYPE_UTILS_H_
#define EXTENSIONS_BROWSER_VIEW_TYPE_UTILS_H_

#include "extensions/common/mojom/view_type.mojom.h"

namespace content {
class WebContents;
}

namespace extensions {

// Get/Set the type of a WebContents.
// GetViewType handles a NULL |tab| for convenience by returning
// mojom::ViewType::kInvalid.
mojom::ViewType GetViewType(content::WebContents* tab);
void SetViewType(content::WebContents* tab, mojom::ViewType type);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_VIEW_TYPE_UTILS_H_
