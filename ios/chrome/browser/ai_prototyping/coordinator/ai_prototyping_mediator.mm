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
#import "ios/chrome/browser/ai_prototyping/model/tab_organization_service_impl.h"
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

  // Mojo related service and service implementations. Kept alive to have an
  // existing implementation instance during the lifecycle of the mediator.
  // Remote used to make calls to functions related to `AIPrototypingService`.
  mojo::Remote<ai::mojom::AIPrototypingService> _ai_prototyping_service;
  // Instantiated to pipe virtual remote calls to overridden functions in the
  // `AIPrototypingServiceImpl`.
  std::unique_ptr<ai::AIPrototypingServiceImpl> _ai_prototyping_service_impl;

  // Remote used to make calls to functions related to `TabOrganizationService`.
  mojo::Remote<ai::mojom::TabOrganizationService> _tab_organization_service;
  // Instantiated to pipe virtual remote calls to overridden functions in the
  // `TabOrganizationServiceImpl`.
  std::unique_ptr<ai::TabOrganizationServiceImpl>
      _tab_organization_service_impl;

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  // The Tab Organization feature's request wrapper.
  TabOrganizationRequestWrapper* _tabOrganizationRequestWrapper;
#endif
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;

    bool startOnDevice = false;
    mojo::PendingReceiver<ai::mojom::AIPrototypingService>
        ai_prototyping_receiver =
            _ai_prototyping_service.BindNewPipeAndPassReceiver();
    web::BrowserState* browserState =
        _webStateList->GetActiveWebState()->GetBrowserState();
    _ai_prototyping_service_impl =
        std::make_unique<ai::AIPrototypingServiceImpl>(
            std::move(ai_prototyping_receiver), browserState, startOnDevice);

    mojo::PendingReceiver<ai::mojom::TabOrganizationService>
        tab_organization_receiver =
            _tab_organization_service.BindNewPipeAndPassReceiver();
    _tab_organization_service_impl =
        std::make_unique<ai::TabOrganizationServiceImpl>(
            std::move(tab_organization_receiver), _webStateList, startOnDevice);
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

  // Create return callback for `_tab_organization_service`.
  base::OnceCallback<void(const std::string& response_string)>
      service_callback =
          base::BindOnce(^void(const std::string& response_string) {
            [weakSelf.consumer
                updateQueryResult:base::SysUTF8ToNSString(response_string)
                       forFeature:AIPrototypingFeature::kTabOrganization];
          });

  // Create completion callback for TabOrganization request wrapper.
  base::OnceCallback<void(
      std::unique_ptr<optimization_guide::proto::TabOrganizationRequest>)>
      completion_callback = base::BindOnce(
          [](AIPrototypingMediator* mediator,
             base::OnceCallback<void(const std::string& response_string)>
                 callback,
             std::unique_ptr<optimization_guide::proto::TabOrganizationRequest>
                 request) {
            ::mojo_base::ProtoWrapper proto_wrapper =
                mojo_base::ProtoWrapper(*request.get());

            mediator->_tab_organization_service->ExecuteGroupTabs(
                std::move(proto_wrapper), std::move(callback));
          },
          base::Unretained(self), std::move(service_callback));

  // Create the TabOrganization request wrapper, and start populating its
  // fields. When completed, `completionCallback` will be executed.
  _tabOrganizationRequestWrapper = [[TabOrganizationRequestWrapper alloc]
                 initWithWebStateList:_webStateList
      allowReorganizingExistingGroups:true
                     groupingStrategy:strategy
                   completionCallback:std::move(completion_callback)];
  [_tabOrganizationRequestWrapper populateRequestFieldsAsync];
  _tabOrganizationRequestWrapper = nil;

}

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

@end
