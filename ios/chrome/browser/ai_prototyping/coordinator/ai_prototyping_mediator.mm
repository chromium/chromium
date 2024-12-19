// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/coordinator/ai_prototyping_mediator.h"

#import "base/functional/bind.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "ios/chrome/browser/ai_prototyping/model/ai_prototyping_service_impl.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_consumer.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/tab_organization_request_wrapper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/optimization_guide/proto/features/tab_organization.pb.h"
#import "components/optimization_guide/proto/string_value.pb.h"  // nogncheck
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#endif

@implementation AIPrototypingMediator {
  raw_ptr<WebStateList> _webStateList;

  // Remote used to make calls to functions related to `AIPrototypingService`.
  mojo::Remote<ai::mojom::AIPrototypingService> _ai_prototyping_service;
  // Instantiated to pipe virtual remote calls to overridden functions in the
  // `AIPrototypingServiceImpl`. Kept alive to have an existing implementation
  // instance during the lifecycle of the mediator.
  std::unique_ptr<ai::AIPrototypingServiceImpl> _ai_prototyping_service_impl;

// TODO(crbug.com/379908732): Move to a mojo interface.
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  // Service used to execute LLM queries.
  raw_ptr<OptimizationGuideService> _service;

  // Retains the on-device session in memory.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      _on_device_session;

  // The Tab Organization feature's request wrapper.
  TabOrganizationRequestWrapper* _tabOrganizationRequestWrapper;
#endif
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;

// TODO(crbug.com/379908732): Remove when service is fully ported to a mojo
// interface.
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
    _service = OptimizationGuideServiceFactory::GetForProfile(
        ProfileIOS::FromBrowserState(
            _webStateList->GetActiveWebState()->GetBrowserState()));
#endif

    mojo::PendingReceiver<ai::mojom::AIPrototypingService> receiver =
        _ai_prototyping_service.BindNewPipeAndPassReceiver();
    web::BrowserState* browserState =
        _webStateList->GetActiveWebState()->GetBrowserState();
    _ai_prototyping_service_impl =
        std::make_unique<ai::AIPrototypingServiceImpl>(
            std::move(receiver), browserState, /*start_on_device=*/false);
  }
  return self;
}

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#pragma mark - AIPrototypingMutator

- (void)executeServerQuery:
    (optimization_guide::proto::BlingPrototypingRequest)request {
  ::mojo_base::ProtoWrapper proto_wrapper = mojo_base::ProtoWrapper(request);
  __weak __typeof(self) weakSelf = self;

  _ai_prototyping_service->ExecuteServerQuery(
      std::move(proto_wrapper),
      base::BindOnce(^void(const std::string& response_string) {
        [weakSelf.consumer
            updateQueryResult:base::SysUTF8ToNSString(response_string)
                   forFeature:AIPrototypingFeature::kFreeform];
      }));
}

- (void)executeOnDeviceQuery:(optimization_guide::proto::StringValue)request {
  ::mojo_base::ProtoWrapper proto_wrapper = mojo_base::ProtoWrapper(request);
  __weak __typeof(self) weakSelf = self;
  _ai_prototyping_service->ExecuteOnDeviceQuery(
      std::move(proto_wrapper),
      base::BindOnce(^void(const std::string& response_string) {
        [weakSelf.consumer
            updateQueryResult:base::SysUTF8ToNSString(response_string)
                   forFeature:AIPrototypingFeature::kFreeform];
      }));
}

- (void)executeGroupTabsWithStrategy:
    (optimization_guide::proto::
         TabOrganizationRequest_TabOrganizationModelStrategy)strategy {
  __weak __typeof(self) weakSelf = self;

  // Create the TabOrganization request wrapper, and start populating its
  // fields. When completed, `completionCallback` will be executed.
  _tabOrganizationRequestWrapper =
      [[TabOrganizationRequestWrapper alloc]
                     initWithWebStateList:_webStateList
          allowReorganizingExistingGroups:true
                         groupingStrategy:strategy
                       completionCallback:base::BindOnce(^(
                                              std::unique_ptr<
                                                  optimization_guide::proto::
                                                      TabOrganizationRequest>
                                                  request) {
                         [weakSelf
                             onTabOrganizationRequestCreated:std::move(
                                                                 request)];
                       })];
  [_tabOrganizationRequestWrapper populateRequestFieldsAsync];
}

#pragma mark - Private

// Handles the populated tab organization request by passing it to the model
// execution service.
- (void)onTabOrganizationRequestCreated:
    (std::unique_ptr<optimization_guide::proto::TabOrganizationRequest>)
        request {
  // Execute the request.
  __weak __typeof(self) weakSelf = self;
  _service->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kTabOrganization,
      *request.release(),
      /*execution_timeout*/ std::nullopt,
      base::BindOnce(
          ^(optimization_guide::OptimizationGuideModelExecutionResult result,
            std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry) {
            [weakSelf onGroupTabsResponse:std::move(result)];
          }));
  _tabOrganizationRequestWrapper = nil;
}

// Handles the response for a tab organization query.
- (void)onGroupTabsResponse:
    (optimization_guide::OptimizationGuideModelExecutionResult)result {
  std::string response = "";

  // The model doesn't necessarily group every tab, so track the tabs that have
  // been grouped in order to later list the ungrouped tabs.
  NSMutableSet<NSNumber*>* groupedTabIdentifiers = [NSMutableSet set];

  auto parsed = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::TabOrganizationResponse>(
      result.response.value());

  // For each tab group, print its name and the information of each tab within
  // it.
  for (const optimization_guide::proto::TabGroup& tab_group :
       parsed->tab_groups()) {
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
  for (int index = 0; index < _webStateList->count(); ++index) {
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    if (![groupedTabIdentifiers
            containsObject:[NSNumber
                               numberWithInt:webState->GetUniqueIdentifier()
                                                 .identifier()]]) {
      response += base::StringPrintf(
          "- %s (%s)\n", base::UTF16ToUTF8(webState->GetTitle()).c_str(),
          webState->GetVisibleURL().spec().c_str());
    }
  }

  [self.consumer updateQueryResult:base::SysUTF8ToNSString(response)
                        forFeature:AIPrototypingFeature::kTabOrganization];
}

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

@end
