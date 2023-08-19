// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_SHARING_SERVICE_PICKER_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_SHARING_SERVICE_PICKER_H_

#include <AppKit/AppKit.h>

#include <string>

#include "content/common/render_widget_host_ns_view.mojom.h"

namespace remote_cocoa {

void ShowSharingServicePickerForView(
    NSView* owning_view,
    const std::string& title,
    const std::string& text,
    const std::string& url,
    const std::vector<std::string>& file_paths,
    mojom::RenderWidgetHostNSView::ShowSharingServicePickerCallback callback);

}  // namespace remote_cocoa

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_SHARING_SERVICE_PICKER_H_
