// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/coordinator/ai_prototyping_mediator.h"

#import <optional>
#import <string>

#import "base/base64.h"
#import "base/functional/bind.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/logging.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/values.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/optimization_guide/proto/features/enhanced_calendar.pb.h"
#import "components/optimization_guide/proto/features/ios_smart_tab_grouping.pb.h"
#import "components/optimization_guide/proto/string_value.pb.h"  // nogncheck
#import "ios/chrome/browser/ai_prototyping/features.h"
#import "ios/chrome/browser/ai_prototyping/model/ai_prototyping_service_impl.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_consumer.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/browser/ai_prototyping/utils/json_action_parser.h"
#import "ios/chrome/browser/ai_prototyping/utils/page_context_util.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_service_impl.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/ios_smart_tab_grouping_request_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/chrome/browser/intelligence/smart_tab_grouping/model/smart_tab_grouping_service_impl.h"
#import "ios/chrome/browser/intelligence/smart_tab_grouping/utils/smart_tab_grouping_utils.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/mojom/enhanced_calendar_service.mojom-forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

namespace {

std::string GetJournalLogsAsJson(actor::AggregatedJournal* journal) {
  if (!journal) {
    return "{}";
  }
  base::ListValue list;
  for (actor::AggregatedJournal::EntryBuffer::Iterator it = journal->Items();
       it; ++it) {
    const std::unique_ptr<actor::AggregatedJournal::Entry>* entry_ptr = *it;
    if (!entry_ptr || !*entry_ptr || !(*entry_ptr)->data) {
      continue;
    }
    const actor::AggregatedJournal::Entry& entry_wrapper = **entry_ptr;
    const actor::mojom::JournalEntry& entry = *entry_wrapper.data;
    base::DictValue dict;
    switch (entry.type) {
      case actor::mojom::JournalEntryType::kBegin:
        dict.Set("type", "Begin");
        break;
      case actor::mojom::JournalEntryType::kEnd:
        dict.Set("type", "End");
        break;
      case actor::mojom::JournalEntryType::kInstant:
        dict.Set("type", "Instant");
        break;
    }
    dict.Set("task_id", base::NumberToString(entry.task_id.value()));
    dict.Set("event", entry.event);
    dict.Set("timestamp", entry.timestamp.InSecondsFSinceUnixEpoch());
    dict.Set("track_uuid", base::NumberToString(entry.track_uuid));
    if (!entry_wrapper.url.empty()) {
      dict.Set("url", entry_wrapper.url);
    }

    base::ListValue details_list;
    for (const actor::mojom::JournalDetailsPtr& detail : entry.details) {
      if (!detail) {
        continue;
      }
      base::DictValue detail_dict;
      detail_dict.Set("key", detail->key);
      detail_dict.Set("value", detail->value);
      details_list.Append(std::move(detail_dict));
    }
    dict.Set("details", std::move(details_list));
    list.Append(std::move(dict));
  }

  std::string json;
  base::JSONWriter::Write(list, &json);
  return json;
}

}  // namespace

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
                    richExtraction:(BOOL)richExtraction
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
  _pageContextWrapper = CreatePageContextWrapper(
      _webStateList->GetActiveWebState(), richExtraction,
      std::move(page_context_completion_callback));
  PopulatePageContext(_pageContextWrapper, _webStateList->GetActiveWebState());
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
    CHECK(IsUploadBlingAIPrototypingDataEnabled());
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

// Handles the IosSmartTabGroupingResponse by outputting the response proto or
// an error message into the result text field.
- (void)handleSmartTabGroupingResponseResult:
    (ai::mojom::SmartTabGroupingResponseResultPtr)response_result {
  if (response_result->is_error()) {
    [self.consumer
        updateQueryResult:base::SysUTF8ToNSString(response_result->get_error())
               forFeature:AIPrototypingFeature::kSmartTabGrouping];
    return;
  }

  auto response_proto =
      response_result->get_response()
          .As<optimization_guide::proto::IosSmartTabGroupingResponse>();

  if (!response_proto.has_value()) {
    [self.consumer
        updateQueryResult:@"Error parsing IosSmartTabGroupingResponse"
               forFeature:AIPrototypingFeature::kSmartTabGrouping];
    return;
  }

  ApplySmartTabGroupResponse(response_proto.value(), _webStateList);

  std::string result_string =
      [self serializeSmartTabGroupingResponseToString:response_proto.value()];

  [self.consumer updateQueryResult:base::SysUTF8ToNSString(result_string)
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
  if (!kUploadBlingAIPrototypingDataLoggingTag.Get().empty() ||
      !kUploadBlingAIPrototypingDataLoggingDescription.Get().empty()) {
    auto metadata =
        std::make_unique<optimization_guide::proto::BlingPrototypingMetadata>();
    metadata->set_logging_tag(kUploadBlingAIPrototypingDataLoggingTag.Get());
    metadata->set_logging_description(
        kUploadBlingAIPrototypingDataLoggingDescription.Get());
    *proto_logging_data.mutable_metadata() = *metadata;
    NSLog(@"[AIPrototypingMediator] Logging MQLS with logging_tag: %@",
          base::SysUTF8ToNSString(proto_logging_data.metadata().logging_tag()));
  }
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          mqls_service->GetWeakPtr());
  *log_entry->log_ai_data_request()->mutable_bling_prototyping() =
      proto_logging_data;
  optimization_guide::ModelQualityLogEntry::Upload(std::move(log_entry));
}

- (void)serializePageContextToStorage:
    (const optimization_guide::proto::PageContext&)pageContext {
  SavePageContextResult result = SaveSerializedPageContextToDisk(pageContext);
  if (!result.success) {
    NSLog(@"[AIPrototypingMediator] Failed to save serialized page context to "
          @"disk: %@",
          base::SysUTF8ToNSString(result.error_message));
  } else {
    NSLog(@"[AIPrototypingMediator] Successfully saved serialized page context "
          @"to: %@",
          base::SysUTF8ToNSString(result.file_path.value()));
  }
}

- (void)executeActuationWithParams:(NSDictionary*)params {
  actor::ActorService* actorService =
      actor::ActorServiceFactory::GetForProfile(ProfileIOS::FromBrowserState(
          _webStateList->GetActiveWebState()->GetBrowserState()));

  if (!actorService) {
    [self.consumer updateQueryResult:@"Error: ActorService not available."
                          forFeature:AIPrototypingFeature::kActorTools];
    return;
  }

  NSString* jsonString = params[@"json"];
  if (jsonString.length == 0) {
    [self.consumer updateQueryResult:@"Error: No JSON provided."
                          forFeature:AIPrototypingFeature::kActorTools];
    return;
  }
  std::optional<base::Value> jsonVal = base::JSONReader::Read(
      base::SysNSStringToUTF8(jsonString), base::JSON_ALLOW_TRAILING_COMMAS);

  if (!jsonVal) {
    [self.consumer updateQueryResult:@"Error: Invalid JSON."
                          forFeature:AIPrototypingFeature::kActorTools];
    return;
  }

  std::vector<optimization_guide::proto::Action> actions;
  if (jsonVal->is_dict()) {
    optimization_guide::proto::Action action;
    if (!ai_prototyping::ParseActionFromDict(jsonVal->GetDict(), &action)) {
      [self.consumer updateQueryResult:@"Error: Unknown action type in JSON."
                            forFeature:AIPrototypingFeature::kActorTools];
      return;
    }
    actions.push_back(std::move(action));
  } else if (jsonVal->is_list()) {
    for (const auto& item : jsonVal->GetList()) {
      if (!item.is_dict()) {
        [self.consumer updateQueryResult:@"Error: Invalid JSON array element."
                              forFeature:AIPrototypingFeature::kActorTools];
        return;
      }
      optimization_guide::proto::Action action;
      if (!ai_prototyping::ParseActionFromDict(item.GetDict(), &action)) {
        [self.consumer
            updateQueryResult:@"Error: Unknown action type in JSON array."
                   forFeature:AIPrototypingFeature::kActorTools];
        return;
      }
      actions.push_back(std::move(action));
    }
  } else {
    [self.consumer
        updateQueryResult:@"Error: JSON must be a dictionary or list."
               forFeature:AIPrototypingFeature::kActorTools];
    return;
  }

  __weak __typeof(self) weakSelf = self;

  actor::ActorTaskId task_id = actorService->CreateTask(
      "AI Prototyping Test Task", /*allow_incognito_web_states=*/false);

  actor::CreateActorToolRequestsResult tools_result =
      actorService->CreateActorToolRequests(actions, task_id);

  if (!tools_result.has_value()) {
    NSString* errorMsg = base::SysUTF8ToNSString(base::StringPrintf(
        "Failed to create tools: %s",
        actor::GetToolExecutionResultMessage(tools_result.error()).c_str()));
    [self.consumer updateQueryResult:errorMsg
                          forFeature:AIPrototypingFeature::kActorTools];
    actorService->StopTask(task_id, actor::ActorTaskStoppedReason::kModelError);
    return;
  }

  actorService->PerformActions(
      task_id, std::move(tools_result.value()),
      "Executing AI Prototyping actions",
      base::BindOnce(^(actor::PerformActionsResult result) {
        [weakSelf onActionsPerformed:std::move(result.action_results)
                         withActions:actions];
      }));
}

// Aggregates the results of actor actions and updates the consumer / UI.
- (void)onActionsPerformed:(std::vector<actor::ActionResult>)results
               withActions:
                   (const std::vector<optimization_guide::proto::Action>&)
                       actions {
  actor::ActorService* actorService =
      actor::ActorServiceFactory::GetForProfile(ProfileIOS::FromBrowserState(
          _webStateList->GetActiveWebState()->GetBrowserState()));

  NSString* result_text = @"";
  std::string summary_str = "Action Results:\n";

  for (size_t i = 0; i < results.size() && i < actions.size(); ++i) {
    const auto& action = actions[i];
    const auto& result = results[i];

    std::optional<std::string> tool_name_opt =
        actor::ActorActionCaseToToolName(action.action_case());
    std::string tool_name = tool_name_opt.value_or("Unknown tool");

    summary_str += " - " + tool_name + ": ";
    if (result.tool_result.IsOk()) {
      summary_str += "SUCCESS\n";
    } else {
      summary_str +=
          "ERROR: " + actor::GetToolExecutionResultMessage(result.tool_result) +
          "\n";
    }
  }
  std::string json_str = GetJournalLogsAsJson(actorService->GetJournal());

  result_text =
      base::SysUTF8ToNSString(summary_str + "\nJSON journal:\n" + json_str);

  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.consumer updateQueryResult:result_text
                              forFeature:AIPrototypingFeature::kActorTools];
  });
}

- (void)listTabs {
  NSMutableArray<NSDictionary*>* tabs = [NSMutableArray array];
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }

  for (int i = 0; i < _webStateList->count(); ++i) {
    web::WebState* webState = _webStateList->GetWebStateAt(i);
    NSString* title = base::SysUTF16ToNSString(webState->GetTitle());
    int32_t tabID = webState->GetUniqueIdentifier().identifier();
    NSString* url = base::SysUTF8ToNSString(webState->GetVisibleURL().spec());
    [tabs addObject:@{
      @"id" : @(tabID),
      @"title" : title ?: @"",
      @"url" : url ?: @"",
      @"active" : @(webState == activeWebState)
    }];
  }

  if ([self.consumer respondsToSelector:@selector(updateTabList:)]) {
    [self.consumer updateTabList:tabs];
  }
}

// Executes the APC extraction for the active WebState using
// `PageContextWrapper`. The resulting `PageContext` proto is serialized to disk
// in a background thread, and the file path is displayed in the prototyping
// menu.
- (void)executeAPCExtractionWithRichExtraction:(BOOL)useRichExtraction
                                actionableMode:(BOOL)actionableMode
                              includeDebugData:(BOOL)includeDebugData {
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  if (!activeWebState) {
    [self.consumer updateQueryResult:@"Error: No active web state."
                          forFeature:AIPrototypingFeature::kAPC];
    if ([self.consumer respondsToSelector:@selector(updateFrameList:)]) {
      [self.consumer updateFrameList:@[]];
    }
    return;
  }

  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder()
          .SetUseRichExtraction(useRichExtraction)
          .SetUseRichExtractionWithActionable(actionableMode)
          .Build();

  __weak __typeof(self) weakSelf = self;
  auto completion = base::BindOnce(^(
      PageContextWrapperCallbackResponse response) {
    if (!response.has_value()) {
      [weakSelf.consumer
          updateQueryResult:@"Error: Failed to populate PageContext."
                 forFeature:AIPrototypingFeature::kAPC];
      if ([weakSelf.consumer respondsToSelector:@selector(updateFrameList:)]) {
        [weakSelf.consumer updateFrameList:@[]];
      }
      return;
    }

    std::unique_ptr<optimization_guide::proto::PageContext> page_context =
        std::move(response.value());

    if (includeDebugData) {
      [weakSelf getFramesAndContentNodes:*page_context
                             forConsumer:weakSelf.consumer];
    }

    std::string serialized_proto = page_context->SerializeAsString();

    auto write_to_disk_task = base::BindOnce(
        [](std::unique_ptr<optimization_guide::proto::PageContext> context) {
          return SaveSerializedPageContextToDisk(*context);
        },
        std::move(page_context));

    auto save_to_disk_callback =
        base::BindOnce(^(SavePageContextResult result) {
          NSMutableString* outputStr = [NSMutableString string];
          if (result.success) {
            [outputStr
                appendString:@"Instructions:\nFollow the directions at "
                             @"go/readableapc to view the extracted APC.\n\n"];
            [outputStr
                appendFormat:@"Proto saved to:\n%@\n\n",
                             base::SysUTF8ToNSString(result.file_path.value())];
          } else {
            [outputStr appendString:@"Warning: Failed to save to disk.\n\n"];
          }

          [outputStr appendString:@"Proto Base64 Bytes:\n"];

          // Encode the serialized proto to base64 to prevent corruption when
          // displayed in the UI.
          std::string base64_encoded = base::Base64Encode(serialized_proto);
          NSString* base64Str = base::SysUTF8ToNSString(base64_encoded);
          [outputStr appendString:base64Str];

          [weakSelf.consumer updateQueryResult:outputStr
                                    forFeature:AIPrototypingFeature::kAPC];
          if ([weakSelf.consumer
                  respondsToSelector:@selector(updateRawBytes:forFeature:)]) {
            [weakSelf.consumer updateRawBytes:base64Str
                                   forFeature:AIPrototypingFeature::kAPC];
          }
        });

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        std::move(write_to_disk_task), std::move(save_to_disk_callback));
  });

  _pageContextWrapper =
      [[PageContextWrapper alloc] initWithWebState:activeWebState
                                            config:config
                                completionCallback:std::move(completion)];

  _pageContextWrapper.shouldGetAnnotatedPageContent = YES;
  [_pageContextWrapper populatePageContextFieldsAsync];
}

// Retrieves the frames and content nodes from the `PageContext` as needed by
// the `consumer`.
- (void)getFramesAndContentNodes:
            (const optimization_guide::proto::PageContext&)pageContext
                     forConsumer:(id<AIPrototypingConsumer>)consumer {
  if (!pageContext.has_annotated_page_content()) {
    if ([consumer respondsToSelector:@selector
                  (updateFramesAndContentNodesDebugString:)]) {
      [consumer updateFramesAndContentNodesDebugString:
                    @"Error: AnnotatedPageContent is not available."];
    }
    if ([consumer respondsToSelector:@selector(updateFrameList:)]) {
      [consumer updateFrameList:@[]];
    }
    return;
  }
  const auto& apc = pageContext.annotated_page_content();
  NSMutableArray<NSDictionary*>* frames = [NSMutableArray array];

  if (apc.has_main_frame_data()) {
    NSDictionary* mainFrame =
        [self frameDictionaryFromFrameData:apc.main_frame_data()
                               isMainFrame:YES
                                     depth:0];
    if (mainFrame) {
      [frames addObject:mainFrame];
    }
  }

  std::string framesAndContentNodes;
  if (apc.has_main_frame_data() &&
      apc.main_frame_data().has_document_identifier()) {
    framesAndContentNodes += base::StringPrintf(
        "Main Frame [ID: %s]\n",
        apc.main_frame_data().document_identifier().serialized_token().c_str());
  }

  [self extractFramesAndContentNodesFromNode:apc.root_node()
                                  intoFrames:frames
                             intoDebugString:framesAndContentNodes
                                       depth:0];

  if ([consumer respondsToSelector:@selector(updateFrameList:)]) {
    [consumer updateFrameList:frames];
  }
  if ([consumer respondsToSelector:@selector
                (updateFramesAndContentNodesDebugString:)]) {
    [consumer
        updateFramesAndContentNodesDebugString:base::SysUTF8ToNSString(
                                                   framesAndContentNodes)];
  }
}

// Traverses the `PageContext` tree to extract frames and build a debug string.
- (void)extractFramesAndContentNodesFromNode:
            (const optimization_guide::proto::ContentNode&)node
                                  intoFrames:
                                      (NSMutableArray<NSDictionary*>*)frames
                             intoDebugString:(std::string&)debugString
                                       depth:(int)depth {
  std::string indent(depth * 2, ' ');
  debugString += indent;

  const auto& attrs = node.content_attributes();
  if (attrs.has_common_ancestor_dom_node_id()) {
    debugString +=
        base::StringPrintf("[ID: %d] ", attrs.common_ancestor_dom_node_id());
  }

  if (attrs.has_text_data()) {
    std::string text = attrs.text_data().text_content();
    if (text.length() > 50) {
      text = std::string(base::TruncateUTF8ToByteSize(text, 47)) + "...";
    }
    debugString += "\"" + text + "\"";
  } else {
    debugString += optimization_guide::proto::ContentAttributeType_Name(
        attrs.attribute_type());

    if (attrs.has_iframe_data() && attrs.iframe_data().has_frame_data()) {
      const auto& frameData = attrs.iframe_data().frame_data();
      NSDictionary* frame = [self frameDictionaryFromFrameData:frameData
                                                   isMainFrame:NO
                                                         depth:depth + 1];
      if (frame) {
        [frames addObject:frame];
        debugString += base::StringPrintf(
            "\n%s  [Frame ID: %s]", indent.c_str(),
            frameData.document_identifier().serialized_token().c_str());
      }
    }
  }
  debugString += "\n";

  for (const auto& child : node.children_nodes()) {
    [self extractFramesAndContentNodesFromNode:child
                                    intoFrames:frames
                               intoDebugString:debugString
                                         depth:depth + 1];
  }
}

- (NSDictionary*)frameDictionaryFromFrameData:
                     (const optimization_guide::proto::FrameData&)frameData
                                  isMainFrame:(BOOL)isMainFrame
                                        depth:(int)depth {
  if (!frameData.has_document_identifier()) {
    return nil;
  }
  return @{
    @"document_identifier" : base::SysUTF8ToNSString(
        frameData.document_identifier().serialized_token())
        ?: @"",
    @"url" : base::SysUTF8ToNSString(frameData.url()) ?: @"",
    @"is_main_frame" : @(isMainFrame),
    @"depth" : @(depth)
  };
}

@end
