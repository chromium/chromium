// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/base/models/simple_menu_model.h"    // nogncheck
#include "ui/views/controls/menu/menu_runner.h"  // nogncheck
#endif

namespace content {

class ShellWebContentsViewDelegate : public WebContentsViewDelegate {
 public:
  explicit ShellWebContentsViewDelegate(WebContents* web_contents);
  ~ShellWebContentsViewDelegate() override;

  // Overridden from WebContentsViewDelegate:
  void ShowContextMenu(RenderFrameHost* render_frame_host,
                       const ContextMenuParams& params) override;

#if defined(OS_MAC)
  void ActionPerformed(int id);
  NSObject<RenderWidgetHostViewMacDelegate>* CreateRenderWidgetHostViewDelegate(
      content::RenderWidgetHost* render_widget_host,
      bool is_popup) override;
#endif

 private:
  WebContents* web_contents_;
#if defined(OS_MAC)
  ContextMenuParams params_;
#endif

#if defined(TOOLKIT_VIEWS)
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ShellWebContentsViewDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_
