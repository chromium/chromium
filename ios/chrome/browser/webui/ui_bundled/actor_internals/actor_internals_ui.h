// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ACTOR_INTERNALS_ACTOR_INTERNALS_UI_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ACTOR_INTERNALS_ACTOR_INTERNALS_UI_H_

#import <string>

#import "components/actor/public/mojom/actor_internals.mojom.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/pending_remote.h"
#import "mojo/public/cpp/bindings/receiver.h"

class ActorInternalsHandler;
class ProfileIOS;

// The WebUI controller for chrome://actor-internals.
class ActorInternalsUI : public web::WebUIIOSController,
                         public actor_internals::mojom::PageHandlerFactory {
 public:
  ActorInternalsUI(web::WebUIIOS* web_ui, const std::string& host);

  ActorInternalsUI(const ActorInternalsUI&) = delete;
  ActorInternalsUI& operator=(const ActorInternalsUI&) = delete;

  ~ActorInternalsUI() override;

  // actor_internals::mojom::PageHandlerFactory.
  void CreatePageHandler(
      mojo::PendingRemote<actor_internals::mojom::Page> page,
      mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver)
      override;

  void BindInterface(
      mojo::PendingReceiver<actor_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // Held as a raw_ptr since the profile is guaranteed to outlive this
  // controller as its tabs are destroyed synchronously prior to profile
  // destruction.
  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<ActorInternalsHandler> handler_;
  mojo::Receiver<actor_internals::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_ACTOR_INTERNALS_ACTOR_INTERNALS_UI_H_
