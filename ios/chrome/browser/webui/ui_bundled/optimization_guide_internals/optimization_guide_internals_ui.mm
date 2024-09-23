// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/optimization_guide_internals/optimization_guide_internals_ui.h"

#import "components/grit/optimization_guide_internals_resources.h"
#import "components/grit/optimization_guide_internals_resources_map.h"
#import "components/optimization_guide/core/prediction_manager.h"
#import "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals_page_handler_impl.h"
#import "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/webui/resource_path.h"

namespace {
// Creates a WebUI data source for chrome://optimization-guide-internals page.
// Changes to this should be in sync with its non-iOS equivalent
// //components/.../optimization_guide_internals_ui.cc
web::WebUIIOSDataSource* CreateOptimizationGuideInternalsHTMLSource() {
  web::WebUIIOSDataSource* source = web::WebUIIOSDataSource::Create(
      optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost);

  source->SetDefaultResource(
      IDR_OPTIMIZATION_GUIDE_INTERNALS_OPTIMIZATION_GUIDE_INTERNALS_HTML);
  source->UseStringsJs();
  const base::span<const webui::ResourcePath> resources =
      base::make_span(kOptimizationGuideInternalsResources,
                      kOptimizationGuideInternalsResourcesSize);
  for (const auto& resource : resources)
    source->AddResourcePath(resource.path, resource.id);

  return source;
}
}  // namespace

OptimizationGuideInternalsUI::OptimizationGuideInternalsUI(
    web::WebUIIOS* web_ui,
    const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  auto* service = OptimizationGuideServiceFactory::GetForProfile(profile);
  if (!service)
    return;
  optimization_guide_logger_ = service->GetOptimizationGuideLogger();
  web::WebUIIOSDataSource::Add(profile,
                               CreateOptimizationGuideInternalsHTMLSource());
  web_ui->GetWebState()->GetInterfaceBinderForMainFrame()->AddInterface(
      base::BindRepeating(&OptimizationGuideInternalsUI::BindInterface,
                          base::Unretained(this)));
}

OptimizationGuideInternalsUI::~OptimizationGuideInternalsUI() {
  web_ui()->GetWebState()->GetInterfaceBinderForMainFrame()->RemoveInterface(
      "optimization_guide_internals.mojom.PageHandlerFactory");
}

void OptimizationGuideInternalsUI::BindInterface(
    mojo::PendingReceiver<
        optimization_guide_internals::mojom::PageHandlerFactory> receiver) {
  // TODO(crbug.com/40215132): Remove the reset which is needed now since `this`
  // is reused on internals page reloads.
  optimization_guide_internals_page_factory_receiver_.reset();
  optimization_guide_internals_page_factory_receiver_.Bind(std::move(receiver));
}

void OptimizationGuideInternalsUI::CreatePageHandler(
    mojo::PendingRemote<optimization_guide_internals::mojom::Page> page) {
  optimization_guide_internals_page_handler_ =
      std::make_unique<OptimizationGuideInternalsPageHandlerImpl>(
          std::move(page), optimization_guide_logger_);
}

void OptimizationGuideInternalsUI::RequestDownloadedModelsInfo(
    RequestDownloadedModelsInfoCallback callback) {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  auto* service = OptimizationGuideServiceFactory::GetForProfile(profile);
  if (!service) {
    std::move(callback).Run({});
    return;
  }
  optimization_guide::PredictionManager* prediction_manager =
      service->GetPredictionManager();
  std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
      downloaded_models_info =
          prediction_manager->GetDownloadedModelsInfoForWebUI();
  std::move(callback).Run(std::move(downloaded_models_info));
}

void OptimizationGuideInternalsUI::RequestLoggedModelQualityClientIds(
    RequestLoggedModelQualityClientIdsCallback callback) {
  // The features are not enabled on ios. There will be no client ids.
  std::move(callback).Run({});
  return;
}
