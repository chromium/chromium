// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/coordinator/ai_prototyping_mediator.h"

#import <string>

#import "base/functional/bind.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/optimization_guide/proto/features/enhanced_calendar.pb.h"
#import "components/optimization_guide/proto/features/tab_organization.pb.h"
#import "components/optimization_guide/proto/string_value.pb.h"  // nogncheck
#import "ios/chrome/browser/ai_prototyping/model/ai_prototyping_service_impl.h"
#import "ios/chrome/browser/ai_prototyping/model/tab_organization_service_impl.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_consumer.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_service_impl.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/tab_organization_request_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/mojom/enhanced_calendar_service.mojom-forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

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

  // Remote used to make calls to functions related to
  // `EnhancedCalendarService`.
  mojo::Remote<ai::mojom::EnhancedCalendarService> _enhanced_calendar_service;
  // Instantiated to pipe virtual remote calls to overridden functions in
  // `EnhancedCalendarServiceImpl`.
  std::unique_ptr<ai::EnhancedCalendarServiceImpl>
      _enhanced_calendar_service_impl;

  // The Tab Organization feature's request wrapper.
  TabOrganizationRequestWrapper* _tabOrganizationRequestWrapper;

  // The freeform feature's PageContext wrapper.
  PageContextWrapper* _pageContextWrapper;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;

    bool startOnDevice = false;

    // The following mojo receiver and remotes and held here by the same class,
    // which is non-standard, but we are still using mojo to future-proof for
    // eventual out-of-process functionality, and simply for more cross-platform
    // readiness.

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

    mojo::PendingReceiver<ai::mojom::EnhancedCalendarService>
        enhanced_calendar_receiver =
            _enhanced_calendar_service.BindNewPipeAndPassReceiver();
    _enhanced_calendar_service_impl =
        std::make_unique<ai::EnhancedCalendarServiceImpl>(
            std::move(enhanced_calendar_receiver),
            _webStateList->GetActiveWebState());
  }
  return self;
}

#pragma mark - AIPrototypingMutator

- (void)executeFreeformServerQuery:(NSString*)query
                systemInstructions:(NSString*)systemInstructions
                includePageContext:(BOOL)includePageContext
                       temperature:(float)temperature
                             model:
                                 (optimization_guide::proto::
                                      BlingPrototypingRequest_ModelEnum)model {
  optimization_guide::proto::BlingPrototypingRequest request;

  // Set the whitespace-trimmed query on the request.
  NSString* trimmedQuery = [query
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  request.set_query(base::SysNSStringToUTF8(trimmedQuery));

  // Set the whitespace-trimmed system instructions if it isn't empty.
  NSString* trimmedSystemInstructions = [systemInstructions
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  if (trimmedSystemInstructions.length) {
    request.set_system_instructions(
        base::SysNSStringToUTF8(trimmedSystemInstructions));
  }

  // Set the temperature on the request.
  request.set_temperature(temperature);

  // Set the model on the request.
  request.set_model_enum(model);

  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(const std::string&)> handle_response_callback =
      base::BindOnce(^void(const std::string& response_string) {
        [weakSelf.consumer
            updateQueryResult:base::SysUTF8ToNSString(response_string)
                   forFeature:AIPrototypingFeature::kFreeform];
      });

  // Execute the query immediately and early return if `includePageContext` is
  // false.
  if (!includePageContext) {
    ::mojo_base::ProtoWrapper proto_wrapper = mojo_base::ProtoWrapper(request);
    _ai_prototyping_service->ExecuteServerQuery(
        std::move(proto_wrapper), std::move(handle_response_callback));
    return;
  }

  base::OnceCallback<void(
      std::unique_ptr<optimization_guide::proto::PageContext>)>
      page_context_completion_callback = base::BindOnce(
          ^void(std::unique_ptr<optimization_guide::proto::PageContext>
                    page_context) {
            [weakSelf executeServerQueryWithPageContext:std::move(page_context)
                                        freeformRequest:request];
          });

  // Populate the PageContext proto and then execute the query.
  _pageContextWrapper = [[PageContextWrapper alloc]
        initWithWebState:_webStateList->GetActiveWebState()
      completionCallback:std::move(page_context_completion_callback)];
  [_pageContextWrapper setShouldGetInnerText:YES];
  [_pageContextWrapper setShouldGetSnapshot:YES];
  [_pageContextWrapper populatePageContextFieldsAsync];
}

- (void)executeFreeformOnDeviceQuery:
    (optimization_guide::proto::StringValue)request {
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
  // Ensure that tabOrganizationRequestWrapper is reset from previous attempts.
  if (_tabOrganizationRequestWrapper) {
    _tabOrganizationRequestWrapper = nil;
  }

  __weak __typeof(self) weakSelf = self;

  // Create return callback for `_tab_organization_service`.
  base::OnceCallback<void(const std::string& response_string)>
      service_callback =
          base::BindOnce(^void(const std::string& response_string) {
            [weakSelf.consumer
                updateQueryResult:base::SysUTF8ToNSString(response_string)
                       forFeature:AIPrototypingFeature::kTabOrganization];

            // Assign to a strong variable to avoid race condition when setting
            // `_tabOrganizationRequestWrapper` to nil.
            AIPrototypingMediator* strongSelf = weakSelf;
            if (strongSelf) {
              strongSelf->_tabOrganizationRequestWrapper = nil;
            }
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
}

- (void)executeEnhancedCalendarQueryWithPrompt:(NSString*)prompt
                                  selectedText:(NSString*)selectedText {
  // Create and set the request params.
  ai::mojom::EnhancedCalendarServiceRequestParamsPtr request_params =
      ai::mojom::EnhancedCalendarServiceRequestParams::New();
  request_params->selected_text = base::SysNSStringToUTF8(selectedText);
  request_params->surrounding_text = base::SysNSStringToUTF8(selectedText);

  // Set the whitespace-trimmed prompt on the request, if not empty.
  NSString* trimmedPrompt = [prompt
      stringByTrimmingCharactersInSet:[NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]];
  if (trimmedPrompt.length) {
    request_params->optional_prompt = base::SysNSStringToUTF8(trimmedPrompt);
  }

  // The response handling callback.
  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(ai::mojom::EnhancedCalendarResponseResultPtr)>
      handle_response_callback = base::BindOnce(
          ^void(ai::mojom::EnhancedCalendarResponseResultPtr result) {
            [weakSelf handleEnhancedCalendarResponseResult:std::move(result)];
          });

  _enhanced_calendar_service->ExecuteEnhancedCalendarRequest(
      std::move(request_params), std::move(handle_response_callback));
}

#pragma mark - Private

// All async work from the PageContext wrapper has been completed.
- (void)
    executeServerQueryWithPageContext:
        (std::unique_ptr<optimization_guide::proto::PageContext>)page_context
                      freeformRequest:
                          (optimization_guide::proto::BlingPrototypingRequest)
                              freeform_request {
  _pageContextWrapper = nil;
  freeform_request.set_allocated_page_context(page_context.release());

  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(const std::string&)> handle_response_callback =
      base::BindOnce(^void(const std::string& response_string) {
        [weakSelf.consumer
            updateQueryResult:base::SysUTF8ToNSString(response_string)
                   forFeature:AIPrototypingFeature::kFreeform];
      });

  ::mojo_base::ProtoWrapper proto_wrapper =
      mojo_base::ProtoWrapper(freeform_request);
  _ai_prototyping_service->ExecuteServerQuery(
      std::move(proto_wrapper), std::move(handle_response_callback));
}

// Handles the Enhanced Calendar by outputting the response proto or an error
// message into the result text field.
- (void)handleEnhancedCalendarResponseResult:
    (ai::mojom::EnhancedCalendarResponseResultPtr)response_result {
  if (response_result->is_error()) {
    [self.consumer
        updateQueryResult:base::SysUTF8ToNSString(response_result->get_error())
               forFeature:AIPrototypingFeature::kEnhancedCalendar];
    return;
  }

  std::string result =
      [self serializeEnhancedCalendarResponseToString:
                response_result->get_response()
                    .As<optimization_guide::proto::EnhancedCalendarResponse>()
                    .value()];

  [self.consumer updateQueryResult:base::SysUTF8ToNSString(result)
                        forFeature:AIPrototypingFeature::kEnhancedCalendar];
}

// Serializes the Enhanced Calendar response proto into a digestable/debug
// string.
- (std::string)serializeEnhancedCalendarResponseToString:
    (optimization_guide::proto::EnhancedCalendarResponse)response_proto {
  std::string result;

  result +=
      base::StringPrintf("Title: %s\n", response_proto.event_title().c_str());
  result += base::StringPrintf("Summary: %s\n",
                               response_proto.event_summary().c_str());
  result += base::StringPrintf("Start date: %s\n",
                               response_proto.start_date().c_str());
  result += base::StringPrintf("Start time: %s\n",
                               response_proto.start_time().c_str());
  result +=
      base::StringPrintf("End date: %s\n", response_proto.end_date().c_str());
  result +=
      base::StringPrintf("End time: %s\n", response_proto.end_time().c_str());
  result += base::StringPrintf("Location: %s\n",
                               response_proto.event_location().c_str());
  result += base::StringPrintf("All day: %s\n",
                               response_proto.is_all_day() ? "true" : "false");

  optimization_guide::proto::RecurrenceState enum_value =
      static_cast<optimization_guide::proto::RecurrenceState>(
          response_proto.recurrence());
  std::string enum_name =
      optimization_guide::proto::RecurrenceState_Name(enum_value);
  result += base::StringPrintf("Recurrence: %s\n", enum_name);

  return result;
}

@end
