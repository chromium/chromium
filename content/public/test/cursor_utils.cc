// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/cursor_utils.h"

#include "components/input/cursor_manager.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

// static
ui::mojom::CursorType CursorUtils::GetLastCursorForWebContents(
    WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  input::CursorManager* manager = web_contents_impl->GetPrimaryMainFrame()
                                      ->GetRenderWidgetHost()
                                      ->GetRenderWidgetHostViewBase()
                                      ->GetCursorManager();
  return manager->GetLastSetCursorTypeForTesting();
}

}  // namespace content
