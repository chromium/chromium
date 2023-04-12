// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_SHELL_WEB_CLIENT_H_
#define IOS_WEB_SHELL_SHELL_WEB_CLIENT_H_

#include <memory>

#import "ios/web/public/web_client.h"

namespace web {
class ShellBrowserState;
class ShellWebMainParts;

class ShellWebClient : public WebClient {
 public:
  ShellWebClient();

  ShellWebClient(const ShellWebClient&) = delete;
  ShellWebClient& operator=(const ShellWebClient&) = delete;

  ~ShellWebClient() override;

  // WebClient implementation.
  std::unique_ptr<WebMainParts> CreateWebMainParts() override;
  std::string GetUserAgent(UserAgentType type) const override;
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) const override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) const override;
  void BindInterfaceReceiverFromMainFrame(
      WebState* web_state,
      mojo::GenericPendingReceiver receiver) override;
  bool EnableLongPressUIContextMenu() const override;

  ShellBrowserState* browser_state() const;

 private:
  ShellWebMainParts* web_main_parts_;
};

}  // namespace web

#endif  // IOS_WEB_SHELL_SHELL_WEB_CLIENT_H_
