// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_web_contents_view_delegate.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell_devtools_frontend.h"
#include "content/shell/common/shell_switches.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace content {
namespace {

// Model for the "Debug" menu
class ContextMenuModel : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  ContextMenuModel(WebContents* web_contents, const ContextMenuParams& params)
      : ui::SimpleMenuModel(this),
        web_contents_(web_contents),
        params_(params) {
    AddItem(COMMAND_OPEN_DEVTOOLS, u"Inspect Element");
  }

  ContextMenuModel(const ContextMenuModel&) = delete;
  ContextMenuModel& operator=(const ContextMenuModel&) = delete;

  ~ContextMenuModel() override {}

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override { return true; }
  void ExecuteCommand(int command_id, int event_flags) override {
    switch (command_id) {
      case COMMAND_OPEN_DEVTOOLS:
        ShellDevToolsFrontend* devtools_frontend =
            ShellDevToolsFrontend::Show(web_contents_);
        devtools_frontend->Activate();
        devtools_frontend->InspectElementAt(params_.x, params_.y);
        break;
    };
  }

 private:
  enum CommandID { COMMAND_OPEN_DEVTOOLS };

  raw_ptr<WebContents> web_contents_;
  ContextMenuParams params_;
};

}  // namespace

std::unique_ptr<WebContentsViewDelegate> CreateShellWebContentsViewDelegate(
    WebContents* web_contents) {
  return std::make_unique<ShellWebContentsViewDelegate>(web_contents);
}

ShellWebContentsViewDelegate::ShellWebContentsViewDelegate(
    WebContents* web_contents)
    : web_contents_(web_contents) {}

ShellWebContentsViewDelegate::~ShellWebContentsViewDelegate() {}

void ShellWebContentsViewDelegate::ShowContextMenu(
    RenderFrameHost& render_frame_host,
    const ContextMenuParams& params) {
  if (switches::IsRunWebTestsSwitchPresent())
    return;

  gfx::Point screen_point(params.x, params.y);

  // Convert from content coordinates to window coordinates.
  // This code copied from chrome_web_contents_view_delegate_views.cc
  aura::Window* web_contents_window = web_contents_->GetNativeView();
  aura::Window* root_window = web_contents_window->GetRootWindow();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (screen_position_client) {
    screen_position_client->ConvertPointToScreen(web_contents_window,
                                                 &screen_point);
  }

  context_menu_model_ =
      std::make_unique<ContextMenuModel>(web_contents_, params);
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(), views::MenuRunner::CONTEXT_MENU);

  views::Widget* widget = views::Widget::GetWidgetForNativeView(
      web_contents_->GetTopLevelNativeWindow());
  context_menu_runner_->RunMenuAt(
      widget, nullptr, gfx::Rect(screen_point, gfx::Size()),
      views::MenuAnchorPosition::kTopRight, ui::MENU_SOURCE_NONE);
}

}  // namespace content
