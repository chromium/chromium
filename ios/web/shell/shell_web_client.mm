// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/shell_web_client.h"

#import <UIKit/UIKit.h>

#import <string_view>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/web_state.h"
#import "ios/web/shell/shell_web_main_parts.h"
#import "ios/web/shell/web_usage_controller.mojom.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/self_owned_receiver.h"
#import "ui/base/resource/resource_bundle.h"

namespace web {

namespace {

// Implementation of mojom::WebUsageController that exposes the ability
// to set whether a WebState has web usage enabled via Mojo.
class WebUsageController : public mojom::WebUsageController {
 public:
  explicit WebUsageController(WebState* web_state) : web_state_(web_state) {}
  ~WebUsageController() override {}

 private:
  void SetWebUsageEnabled(bool enabled,
                          SetWebUsageEnabledCallback callback) override {
    web_state_->SetWebUsageEnabled(enabled);
    std::move(callback).Run();
  }

  raw_ptr<WebState> web_state_;
};

}  // namespace

ShellWebClient::ShellWebClient() : web_main_parts_(nullptr) {}

ShellWebClient::~ShellWebClient() {
}

std::unique_ptr<web::WebMainParts> ShellWebClient::CreateWebMainParts() {
  auto web_main_parts = std::make_unique<ShellWebMainParts>();
  web_main_parts_ = web_main_parts.get();
  return web_main_parts;
}

ShellBrowserState* ShellWebClient::browser_state() const {
  return web_main_parts_->browser_state();
}

std::string ShellWebClient::GetUserAgent(UserAgentType type) const {
  return web::BuildMobileUserAgent("CriOS/36.77.34.45");
}

std::string_view ShellWebClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ShellWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

void ShellWebClient::BindInterfaceReceiverFromMainFrame(
    WebState* web_state,
    mojo::GenericPendingReceiver receiver) {
  if (auto web_usage_receiver = receiver.As<mojom::WebUsageController>()) {
    mojo::MakeSelfOwnedReceiver(std::make_unique<WebUsageController>(web_state),
                                std::move(web_usage_receiver));
  }
}

bool ShellWebClient::EnableLongPressUIContextMenu() const {
  return true;
}

bool ShellWebClient::EnableWebInspector(BrowserState* browser_state) const {
  return true;
}

}  // namespace web
