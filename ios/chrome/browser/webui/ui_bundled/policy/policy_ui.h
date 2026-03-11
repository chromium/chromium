// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_POLICY_POLICY_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_POLICY_POLICY_UI_H_

#import "base/memory/weak_ptr.h"
#import "base/values.h"
#import "components/policy/resources/webui/mojom/policy.mojom.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "mojo/public/cpp/bindings/receiver.h"

namespace web {
class WebUIIOS;
}

class PolicyUIHandler;

// The Web UI controller for the chrome://policy page.
class PolicyUI : public web::WebUIIOSController,
                 policy::mojom::PolicyPageHandlerFactory {
 public:
  explicit PolicyUI(web::WebUIIOS* web_ui, const std::string& host);
  ~PolicyUI() override;
  PolicyUI(const PolicyUI&) = delete;
  PolicyUI& operator=(const PolicyUI&) = delete;

  static bool ShouldLoadTestPage(ProfileIOS* profile);
  static base::Value GetSchema(ProfileIOS* profile);

  void BindInterface(
      mojo::PendingReceiver<policy::mojom::PolicyPageHandlerFactory> receiver);

 private:
  void CreateHandler(
      mojo::PendingReceiver<policy::mojom::PolicyPageHandler> handler,
      mojo::PendingRemote<policy::mojom::PolicyPageClient> client) override;

  mojo::Receiver<policy::mojom::PolicyPageHandlerFactory> receiver_{this};
  std::unique_ptr<PolicyUIHandler> handler_;

  base::WeakPtrFactory<PolicyUI> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_POLICY_POLICY_UI_H_
