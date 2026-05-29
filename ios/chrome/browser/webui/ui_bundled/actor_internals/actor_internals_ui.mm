// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/actor_internals/actor_internals_ui.h"

#import "base/containers/span.h"
#import "components/grit/actor_internals_resources.h"
#import "components/grit/actor_internals_resources_map.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/webui/ui_bundled/actor_internals/actor_internals_handler.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

ActorInternalsUI::ActorInternalsUI(web::WebUIIOS* web_ui,
                                   const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  profile_ = ProfileIOS::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIActorInternalsHost);

  for (const auto& resource : kActorInternalsResources) {
    source->AddResourcePath(resource.path, resource.id);
  }
  source->SetDefaultResource(IDR_ACTOR_INTERNALS_ACTOR_INTERNALS_HTML);

  web::WebUIIOSDataSource::Add(profile_, source);

  web_ui->GetWebState()->GetInterfaceBinderForMainFrame()->AddInterface(
      base::BindRepeating(
          &ActorInternalsUI::BindInterface,
          // It is safe to use `base::Unretained(this)` because the interface is
          // removed in the destructor, ensuring no callbacks will be executed
          // after `this` is destroyed.
          base::Unretained(this)));
}

ActorInternalsUI::~ActorInternalsUI() {
  web_ui()->GetWebState()->GetInterfaceBinderForMainFrame()->RemoveInterface(
      "actor_internals.mojom.PageHandlerFactory");
}

void ActorInternalsUI::BindInterface(
    mojo::PendingReceiver<actor_internals::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ActorInternalsUI::CreatePageHandler(
    mojo::PendingRemote<actor_internals::mojom::Page> page,
    mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver) {
  actor::ActorService* service =
      actor::ActorServiceFactory::GetForProfile(profile_);
  actor::AggregatedJournal* journal = service ? service->GetJournal() : nullptr;
  handler_ = std::make_unique<ActorInternalsHandler>(journal, std::move(page),
                                                     std::move(receiver));
}
