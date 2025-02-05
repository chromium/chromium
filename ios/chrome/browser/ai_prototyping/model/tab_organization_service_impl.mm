// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/model/tab_organization_service_impl.h"

#import "base/strings/stringprintf.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/tab_organization_request_wrapper.h"
#import "ios/chrome/browser/optimization_guide/mojom/tab_organization_service.mojom.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#import "components/optimization_guide/proto/features/tab_organization.pb.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"

namespace ai {

TabOrganizationServiceImpl::TabOrganizationServiceImpl(
    mojo::PendingReceiver<mojom::TabOrganizationService> receiver,
    WebStateList* web_state_list,
    bool start_on_device)
    : receiver_(this, std::move(receiver)) {
  web_state_list_ = web_state_list;
  service_ = OptimizationGuideServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(
          web_state_list->GetActiveWebState()->GetBrowserState()));
}

TabOrganizationServiceImpl::~TabOrganizationServiceImpl() = default;

void TabOrganizationServiceImpl::ExecuteGroupTabs(
    ::mojo_base::ProtoWrapper request,
    ExecuteGroupTabsCallback callback) {
  optimization_guide::proto::TabOrganizationRequest proto_request =
      request.As<optimization_guide::proto::TabOrganizationRequest>().value();

  optimization_guide::OptimizationGuideModelExecutionResultCallback
      result_callback = base::BindOnce(
          [](ExecuteGroupTabsCallback tabs_callback,
             TabOrganizationServiceImpl* service,
             optimization_guide::OptimizationGuideModelExecutionResult result,
             std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry) {
            std::string response =
                service->OnGroupTabsResponse(std::move(result));
            std::move(tabs_callback).Run(response);
          },
          std::move(callback), base::Unretained(this));

  service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kTabOrganization,
      proto_request,
      /*execution_timeout*/ std::nullopt, std::move(result_callback));
}

std::string TabOrganizationServiceImpl::OnGroupTabsResponse(
    optimization_guide::OptimizationGuideModelExecutionResult result) {
  std::string response = "";

  if (!result.response.has_value()) {
    return base::StrCat({"Server model execution error: ",
                         service_->ResponseForErrorCode(static_cast<int>(
                             result.response.error().error()))});
  }

  auto tab_organization_response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::TabOrganizationResponse>(
      result.response.value());

  if (tab_organization_response->tab_groups().empty()) {
    return "No grouped tabs returned.";
  }

  const google::protobuf::RepeatedPtrField<optimization_guide::proto::TabGroup>&
      tab_groups = tab_organization_response->tab_groups();

  // The model doesn't necessarily group every tab, so track the tabs that have
  // been grouped in order to later list the ungrouped tabs.
  NSMutableSet<NSNumber*>* groupedTabIdentifiers = [NSMutableSet set];

  // For each tab group, print its name and the information of each tab within
  // it.
  for (const optimization_guide::proto::TabGroup& tab_group : tab_groups) {
    response +=
        base::StringPrintf("Group name: %s\n", tab_group.label().c_str());

    for (const optimization_guide::proto::Tab& tab : tab_group.tabs()) {
      response += base::StringPrintf("- %s (%s)\n", tab.title().c_str(),
                                     tab.url().c_str());
      [groupedTabIdentifiers addObject:[NSNumber numberWithInt:tab.tab_id()]];
    }
    response += "\n";
  }

  // Find the tabs that haven't been grouped, and print them under "Ungrouped
  // tabs".
  response += "\nUngrouped tabs:\n";
  for (int index = 0; index < web_state_list_->count(); ++index) {
    web::WebState* webState = web_state_list_->GetWebStateAt(index);
    if (![groupedTabIdentifiers
            containsObject:[NSNumber
                               numberWithInt:webState->GetUniqueIdentifier()
                                                 .identifier()]]) {
      response += base::StringPrintf(
          "- %s (%s)\n", base::UTF16ToUTF8(webState->GetTitle()).c_str(),
          webState->GetVisibleURL().spec().c_str());
    }
  }

  return response;
}

}  // namespace ai
