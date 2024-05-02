// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"

#if defined(SHELL_USE_TOOLKIT_VIEWS)
#include "ui/base/models/simple_menu_model.h"    // nogncheck
#include "ui/views/controls/menu/menu_runner.h"  // nogncheck
#endif

namespace content {

class ShellWebContentsUIButtonHolder;

class ShellWebContentsViewDelegate : public WebContentsViewDelegate {
 public:
  explicit ShellWebContentsViewDelegate(WebContents* web_contents);

  ShellWebContentsViewDelegate(const ShellWebContentsViewDelegate&) = delete;
  ShellWebContentsViewDelegate& operator=(const ShellWebContentsViewDelegate&) =
      delete;

  ~ShellWebContentsViewDelegate() override;

  // Overridden from WebContentsViewDelegate:
  void ShowContextMenu(RenderFrameHost& render_frame_host,
                       const ContextMenuParams& params) override;

#if BUILDFLAG(IS_MAC)
  void ActionPerformed(int id);
  NSObject<RenderWidgetHostViewMacDelegate>* GetDelegateForHost(
      content::RenderWidgetHost* render_widget_host,
      bool is_popup) override;
#endif

 private:
  raw_ptr<WebContents> web_contents_;
#if BUILDFLAG(IS_MAC)
  ContextMenuParams params_;
#endif

#if BUILDFLAG(IS_IOS)
  // A hidden button used for displaying context menus.
  std::unique_ptr<ShellWebContentsUIButtonHolder> hidden_button_;
#endif

#if defined(SHELL_USE_TOOLKIT_VIEWS)
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
#endif
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_
