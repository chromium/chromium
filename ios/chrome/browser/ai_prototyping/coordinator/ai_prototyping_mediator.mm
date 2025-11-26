// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/coordinator/ai_prototyping_mediator.h"

#import <string>

#import "base/functional/bind.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/optimization_guide/proto/features/enhanced_calendar.pb.h"
#import "components/optimization_guide/proto/features/ios_smart_tab_grouping.pb.h"
#import "components/optimization_guide/proto/features/tab_organization.pb.h"
#import "components/optimization_guide/proto/string_value.pb.h"  // nogncheck
#import "ios/chrome/browser/ai_prototyping/model/ai_prototyping_service_impl.h"
#import "ios/chrome/browser/ai_prototyping/model/tab_organization_service_impl.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_consumer.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_service_impl.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/ios_smart_tab_grouping_request_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/tab_organization_request_wrapper.h"
#import "ios/chrome/browser/intelligence/smart_tab_grouping/model/smart_tab_grouping_service_impl.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/mojom/enhanced_calendar_service.mojom-forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

@implementation AIPrototypingMediator {
  // Browser agent responsible for persisting and retrieving tab context data.
  raw_ptr<PersistTabContextBrowserAgent> _persistTabContextBrowserAgent;
  // The list of web states in the current browser window.
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

  // Remote used to make calls to functions related to
  // 'SmartTabGroupingService'.
  mojo::Remote<ai::mojom::SmartTabGroupingService> _smartTabGroupingService;
  // Instatiated to pipe virtual remote calls to overriden functions in
  // 'SmartTabGroupingServiceImpl'.
  std::unique_ptr<ai::SmartTabGroupingServiceImpl> _smartTabGroupingServiceImpl;

  // The Tab Organization feature's request wrapper.
  TabOrganizationRequestWrapper* _tabOrganizationRequestWrapper;

  // The freeform feature's PageContext wrapper.
  PageContextWrapper* _pageContextWrapper;

  // Whether to upload data to MQLS.
  BOOL _enableMQLSUpload;

  // Whether to store page context locally.
  BOOL _storePageContextLocally;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
       persistTabContextBrowserAgent:
           (PersistTabContextBrowserAgent*)persistTabContextBrowserAgent {
  self = [super init];
  if (self) {
    _persistTabContextBrowserAgent = persistTabContextBrowserAgent;
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

    mojo::PendingReceiver<ai::mojom::SmartTabGroupingService>
        smartTabGroupingReceiver =
            _smartTabGroupingService.BindNewPipeAndPassReceiver();
    _smartTabGroupingServiceImpl =
        std::make_unique<ai::SmartTabGroupingServiceImpl>(
            std::move(smartTabGroupingReceiver), _webStateList,
            _persistTabContextBrowserAgent);
  }
  return self;
}

#pragma mark - AIPrototypingMutator

- (void)executeFreeformServerQuery:(NSString*)query
                systemInstructions:(NSString*)systemInstructions
                includePageContext:(BOOL)includePageContext
                      uploadToMQLS:(BOOL)uploadToMQLS
                  storePageContext:(BOOL)storePageContext
                       temperature:(float)temperature
                             model:
                                 (optimization_guide::proto::
                                      BlingPrototypingRequest_ModelEnum)model {
  _enableMQLSUpload = uploadToMQLS;
  _storePageContextLocally = storePageContext;

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
  base::OnceCallback<void(const std::string&, ::mojo_base::ProtoWrapper)>
      handle_response_callback =
          base::BindOnce(^void(const std::string& response_string,
                               ::mojo_base::ProtoWrapper logging_data) {
            [weakSelf
                handleFreeformServerQueryResponse:std::move(response_string)
                                  withLoggingData:std::move(logging_data)];
          });

  // Execute the query immediately and early return if `includePageContext` is
  // false.
  if (!includePageContext) {
    ::mojo_base::ProtoWrapper proto_wrapper = mojo_base::ProtoWrapper(request);
    _ai_prototyping_service->ExecuteServerQuery(
        std::move(proto_wrapper), std::move(handle_response_callback));
    return;
  }

  base::OnceCallback<void(PageContextWrapperCallbackResponse)>
      page_context_completion_callback =
          base::BindOnce(^void(PageContextWrapperCallbackResponse response) {
            // TODO(crbug.com/425736226): Handle PageContextWrapper errors.
            if (response.has_value()) {
              [weakSelf
                  executeServerQueryWithPageContext:std::move(response.value())
                                    freeformRequest:request];
            }
          });

  // Populate the PageContext proto and then execute the query.
  _pageContextWrapper = [[PageContextWrapper alloc]
        initWithWebState:_webStateList->GetActiveWebState()
      completionCallback:std::move(page_context_completion_callback)];
  [_pageContextWrapper setShouldGetAnnotatedPageContent:YES];
  [_pageContextWrapper setShouldGetSnapshot:YES];
  [_pageContextWrapper setShouldGetFullPagePDF:YES];
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

- (void)executeSmartTabGrouping {
  __weak __typeof(self) weakSelf = self;
  auto handleResponseBlock =
      ^void(ai::mojom::SmartTabGroupingResponseResultPtr result) {
        [weakSelf handleSmartTabGroupingResponseResult:std::move(result)];
      };

  base::OnceCallback<void(ai::mojom::SmartTabGroupingResponseResultPtr)>
      handleResponseCallback = base::BindOnce(handleResponseBlock);

  // Call the service to execute the request, the service will handle the
  // request population.
  _smartTabGroupingService->ExecuteSmartTabGroupingRequest(
      std::move(handleResponseCallback));
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

  if (_storePageContextLocally) {
    [self serializePageContextToStorage:freeform_request.page_context()];
  }

  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(const std::string&, ::mojo_base::ProtoWrapper)>
      handle_response_callback =
          base::BindOnce(^void(const std::string& response_string,
                               ::mojo_base::ProtoWrapper logging_data) {
            [weakSelf
                handleFreeformServerQueryResponse:std::move(response_string)
                                  withLoggingData:std::move(logging_data)];
          });

  ::mojo_base::ProtoWrapper proto_wrapper =
      mojo_base::ProtoWrapper(freeform_request);
  _ai_prototyping_service->ExecuteServerQuery(
      std::move(proto_wrapper), std::move(handle_response_callback));
}

- (void)handleFreeformServerQueryResponse:(const std::string&)response_string
                          withLoggingData:
                              (::mojo_base::ProtoWrapper)logging_data {
  [self.consumer updateQueryResult:base::SysUTF8ToNSString(response_string)
                        forFeature:AIPrototypingFeature::kFreeform];
  if (_enableMQLSUpload) {
    [self uploadLoggingDataToMQLS:std::move(logging_data)];
  }
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

// Handles the SmartTabGroupingResponse by outputting the response proto or
// an error message into the result text field.
- (void)handleSmartTabGroupingResponseResult:
    (ai::mojom::SmartTabGroupingResponseResultPtr)response_result {
  if (response_result->is_error()) {
    [self.consumer
        updateQueryResult:base::SysUTF8ToNSString(response_result->get_error())
               forFeature:AIPrototypingFeature::kSmartTabGrouping];
    return;
  }

  std::string result = [self
      serializeSmartTabGroupingResponseToString:
          response_result->get_response()
              .As<optimization_guide::proto::IosSmartTabGroupingResponse>()
              .value()];

  [self.consumer updateQueryResult:base::SysUTF8ToNSString(result)
                        forFeature:AIPrototypingFeature::kSmartTabGrouping];
}

// Serializes the IosSmartTabGroupingResponse proto into a human-readable
// string.
- (std::string)serializeSmartTabGroupingResponseToString:
    (const optimization_guide::proto::IosSmartTabGroupingResponse&)
        response_proto {
  std::string result;
  result += "iOS Smart Tab Grouping Response:\n\n";

  for (const auto& group : response_proto.tab_groups()) {
    result += base::StringPrintf("Group: %s %s\n", group.emoji().c_str(),
                                 group.label().c_str());

    std::vector<std::string> tab_ids_str;
    for (int64_t tab_id : group.tab_ids()) {
      tab_ids_str.push_back(base::NumberToString(tab_id));
    }
    result += "Tabs: ";
    for (size_t i = 0; i < tab_ids_str.size(); ++i) {
      result += tab_ids_str[i];
      if (i < tab_ids_str.size() - 1) {
        result += ", ";
      }
    }
    result += "\n\n";
  }

  return result;
}

- (void)uploadLoggingDataToMQLS:(::mojo_base::ProtoWrapper)logging_data {
  // If MQLS toggle is not enabled, ignore the data.
  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(
              _webStateList->GetActiveWebState()->GetBrowserState()));
  if (!optimization_guide_service) {
    return;
  }
  auto* mqls_service =
      optimization_guide_service->GetModelQualityLogsUploaderService();
  if (!mqls_service) {
    return;
  }
  if (!logging_data.As<optimization_guide::proto::BlingPrototypingLoggingData>()
           .has_value()) {
    return;
  }
  optimization_guide::proto::BlingPrototypingLoggingData proto_logging_data =
      logging_data.As<optimization_guide::proto::BlingPrototypingLoggingData>()
          .value();
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          mqls_service->GetWeakPtr());
  *log_entry->log_ai_data_request()->mutable_bling_prototyping() =
      proto_logging_data;
  optimization_guide::ModelQualityLogEntry::Upload(std::move(log_entry));
}

- (void)serializePageContextToStorage:
    (const optimization_guide::proto::PageContext&)pageContext {
  // Get the Documents directory path
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSString* documentsDirectory = [paths objectAtIndex:0];

  NSString* urlString = base::SysUTF8ToNSString(pageContext.url());
  NSCharacterSet* illegalFileNameCharacters =
      [NSCharacterSet characterSetWithCharactersInString:@"/\\?%*|\"<>:"];
  NSString* fileName = [[[urlString
      componentsSeparatedByCharactersInSet:illegalFileNameCharacters]
      componentsJoinedByString:@""] stringByAppendingString:@".txtpb"];

  NSString* filePath =
      [documentsDirectory stringByAppendingPathComponent:fileName];

  NSLog(@"NICMAC Attempting to save proto to: %@", filePath);

  // Convert NSString path to a C_style string for fopen
  std::string UTF8FilePath = base::SysNSStringToUTF8(filePath);
  const char* cFilePath = UTF8FilePath.c_str();
  if (cFilePath == nullptr) {
    NSLog(@"NICMAC Error: Could not convert file path to C_style string.");
    return;
  }

  // Open the file for writing in binary mode and get the file descriptor
  FILE* fp = fopen(cFilePath, "wb");
  if (fp == nullptr) {
    NSLog(@"NICMAC Error: Could not open file '%s' for writing. Error: %s",
          cFilePath, strerror(errno));
    return;
  }

  // Get the file descriptor from the FILE pointer
  int fd = fileno(fp);
  if (fd == -1) {
    NSLog(@"NICMAC Error: Could not get file descriptor for '%s'. Error: %s",
          cFilePath, strerror(errno));
    fclose(fp);
    return;
  }

  // Serialize and write the message to the file
  bool success = pageContext.SerializeToFileDescriptor(fd);

  // Close the file
  if (fclose(fp) != 0) {
    NSLog(@"NICMAC Error: Could not close file '%s' properly. Error: %s",
          cFilePath, strerror(errno));
  }

  if (!success) {
    NSLog(@"NICMAC Error: Failed to serialize protobuf message to file: %@",
          filePath);
    // Delete the file if serialization failed partway
    [[NSFileManager defaultManager] removeItemAtPath:filePath error:nil];
    return;
  }

  NSLog(@"NICMAC Successfully saved protobuf message.");
}

@end
