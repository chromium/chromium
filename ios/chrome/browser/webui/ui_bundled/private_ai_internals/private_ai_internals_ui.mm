// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/private_ai_internals/private_ai_internals_ui.h"

#import "base/functional/bind.h"
#import "components/grit/private_ai_internals_resources.h"
#import "components/grit/private_ai_internals_resources_map.h"
#import "components/private_ai/features.h"
#import "components/private_ai/private_ai_internals/webui/private_ai_internals_page_handler.h"
#import "components/private_ai/private_ai_internals/webui/url_constants.h"
#import "components/private_ai/private_ai_service.h"
#import "ios/chrome/browser/private_ai/model/private_ai_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/webui/resource_path.h"

namespace {
web::WebUIIOSDataSource* CreatePrivateAiInternalsHTMLSource() {
  web::WebUIIOSDataSource* source = web::WebUIIOSDataSource::Create(
      private_ai_internals::kChromeUIPrivateAiInternalsHost);

  source->SetDefaultResource(
      IDR_PRIVATE_AI_INTERNALS_PRIVATE_AI_INTERNALS_HTML);
  source->UseStringsJs();
  const base::span<const webui::ResourcePath> resources(
      kPrivateAiInternalsResources);
  for (const auto& resource : resources) {
    source->AddResourcePath(resource.path, resource.id);
  }

  source->AddString("default_url", private_ai::kPrivateAiUrl.Get());
  source->AddString(
      "default_api_key",
      private_ai::PrivateAiInternalsPageHandler::kApiKeyPlaceholder);
  source->AddString("default_proxy_url",
                    private_ai::kPrivateAiProxyServerUrl.Get());
  source->AddBoolean(
      "default_use_token_attestation",
      base::FeatureList::IsEnabled(private_ai::kPrivateAiUseTokenAttestation));

  return source;
}
}  // namespace

PrivateAiInternalsUI::PrivateAiInternalsUI(web::WebUIIOS* web_ui,
                                           const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(profile, CreatePrivateAiInternalsHTMLSource());
  web_ui->GetWebState()->GetInterfaceBinderForMainFrame()->AddInterface(
      base::BindRepeating(&PrivateAiInternalsUI::BindInterface,
                          base::Unretained(this)));
}

PrivateAiInternalsUI::~PrivateAiInternalsUI() {
  web_ui()->GetWebState()->GetInterfaceBinderForMainFrame()->RemoveInterface(
      "private_ai_internals.mojom.PrivateAiInternalsPageHandler");
}

void PrivateAiInternalsUI::BindInterface(
    mojo::PendingReceiver<
        private_ai_internals::mojom::PrivateAiInternalsPageHandler> receiver) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  auto* private_ai_service = PrivateAiServiceFactory::GetForProfile(profile);

  if (!private_ai_service) {
    return;
  }
  auto* token_manager = private_ai_service->GetTokenManager();
  if (!token_manager) {
    return;
  }

  if (!network_context_.is_bound()) {
    network_driver_ios_.CreateNetworkContext(
        network_context_.BindNewPipeAndPassReceiver(),
        network::mojom::NetworkContextParams::New());
  }

  page_handler_ = std::make_unique<private_ai::PrivateAiInternalsPageHandler>(
      token_manager, network_context_.get(), private_ai_service->GetClient(),
      private_ai_service->GetLogger(), &oak_session_driver_ios_,
      &network_driver_ios_, ::GetChannel(), std::move(receiver));
}
