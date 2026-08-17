// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool.h"

#import <optional>

#import "base/barrier_closure.h"
#import "base/functional/bind.h"
#import "base/notimplemented.h"
#import "base/notreached.h"
#import "base/strings/string_number_conversions.h"
#import "base/types/expected.h"
#import "components/autofill/core/browser/actor/actor_form_filling_service.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_task_form_filling_handler.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// Enum type conversion for RequestedData.
autofill::ActorFormFillingRequestedData MapRequestedData(
    optimization_guide::proto::FormFillingRequest_RequestedData proto_enum) {
  using RequestedData = autofill::ActorFormFillingRequestedData;
  switch (proto_enum) {
    case optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS:
      return RequestedData::kAddress;
    case optimization_guide::proto::
        FormFillingRequest_RequestedData_BILLING_ADDRESS:
      return RequestedData::kBillingAddress;
    case optimization_guide::proto::
        FormFillingRequest_RequestedData_SHIPPING_ADDRESS:
      return RequestedData::kShippingAddress;
    case optimization_guide::proto::
        FormFillingRequest_RequestedData_WORK_ADDRESS:
      return RequestedData::kWorkAddress;
    case optimization_guide::proto::
        FormFillingRequest_RequestedData_HOME_ADDRESS:
      return RequestedData::kHomeAddress;
    case optimization_guide::proto::
        FormFillingRequest_RequestedData_CREDIT_CARD:
      return RequestedData::kCreditCard;
    case optimization_guide::proto::
        FormFillingRequest_RequestedData_CONTACT_INFORMATION:
      return RequestedData::kContactInformation;
    default:
      NOTIMPLEMENTED();
      return RequestedData::kUnknown;
  }
}

// Converts the `FormFillingRequest` proto to a `FormFillingRequest`.
std::optional<actor::FormFillingRequest> ToFormFillingRequest(
    const optimization_guide::proto::FormFillingRequest& request_proto) {
  if (request_proto.trigger_fields().empty()) {
    return std::nullopt;
  }
  actor::FormFillingRequest request;
  request.requested_data = MapRequestedData(request_proto.requested_data());
  if (request_proto.has_section_label()) {
    // This field would not be used unless we introduce aggregated card.
    request.section_label = request_proto.section_label();
  }
  request.trigger_fields.reserve(request_proto.trigger_fields_size());

  for (const auto& trigger_field : request_proto.trigger_fields()) {
    actor::ActionTarget target = actor::ActionTarget::FromProto(trigger_field);
    if (!target.is_valid()) {
      return std::nullopt;
    }
    request.trigger_fields.push_back(std::move(target));
  }
  return request;
}

// Converts an autofill service error into a tool execution result.
actor::ToolExecutionResult FromServiceError(
    autofill::ActorFormFillingError error) {
  using enum autofill::ActorFormFillingError;
  switch (error) {
    case kAutofillNotAvailable:
      return actor::ToolExecutionResult(
          actor::mojom::ActionResultCode::kFormFillingAutofillUnavailable,
          /*requires_page_stabilization=*/false, "Autofill is not available.");
    case kNoSuggestions:
      return actor::ToolExecutionResult(
          actor::mojom::ActionResultCode::kFormFillingNoSuggestionsAvailable,
          /*requires_page_stabilization=*/false,
          "No autofill suggestions available for the fields.");
    case kNoForm:
      return actor::ToolExecutionResult(
          actor::mojom::ActionResultCode::kObservedTargetElementDestroyed,
          /*requires_page_stabilization=*/false,
          "The form was not found or has changed.");
    case kOther:
      return actor::ToolExecutionResult(
          actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError,
          /*requires_page_stabilization=*/false,
          "An unknown error occurred in the form filling service.");
  }
  NOTREACHED();
}

}  // namespace

namespace actor {

#pragma mark - FormFillingRequest

FormFillingRequest::FormFillingRequest() = default;
FormFillingRequest::~FormFillingRequest() = default;
FormFillingRequest::FormFillingRequest(const FormFillingRequest&) = default;
FormFillingRequest& FormFillingRequest::operator=(const FormFillingRequest&) =
    default;
FormFillingRequest::FormFillingRequest(FormFillingRequest&&) = default;
FormFillingRequest& FormFillingRequest::operator=(FormFillingRequest&&) =
    default;

#pragma mark - AttemptFormFillingTool

// static
std::unique_ptr<AttemptFormFillingTool> AttemptFormFillingTool::Create(
    base::WeakPtr<web::WebState> web_state,
    const optimization_guide::proto::AttemptFormFillingAction& action,
    ToolDelegate* tool_delegate) {
  std::vector<FormFillingRequest> requests;
  for (const auto& request_proto : action.form_filling_requests()) {
    std::optional<FormFillingRequest> request =
        ToFormFillingRequest(request_proto);
    if (!request.has_value()) {
      requests.clear();
      break;
    }
    requests.push_back(request.value());
  }
  return std::unique_ptr<AttemptFormFillingTool>(new AttemptFormFillingTool(
      web_state, std::move(requests), tool_delegate));
}

AttemptFormFillingTool::AttemptFormFillingTool(
    base::WeakPtr<web::WebState> web_state,
    std::vector<FormFillingRequest> requests,
    ToolDelegate* tool_delegate)
    : web_state_(web_state),
      tool_requests_(std::move(requests)),
      tool_delegate_(tool_delegate) {}

AttemptFormFillingTool::~AttemptFormFillingTool() = default;

void AttemptFormFillingTool::Validate(ToolExecutionCallback callback) {
  if (tool_requests_.empty()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }
  for (const FormFillingRequest& tool_request : tool_requests_) {
    if (tool_request.trigger_fields.empty()) {
      std::move(callback).Run(
          ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
      return;
    }
  }
  std::move(callback).Run(ToolExecutionResult::Ok());
}

void AttemptFormFillingTool::Execute(ToolExecutionCallback callback) {
  if (!web_state_) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  web::WebFramesManager* frames_manager =
      ActionTargetJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_.get());
  if (!frames_manager || !frames_manager->GetMainWebFrame()) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  execute_callback_ = std::move(callback);

  // Pre-allocate the resolved requests.
  CHECK(!tool_requests_.empty());
  CHECK(service_requests_.empty());
  CHECK(frame_to_indices_map_.empty());
  service_requests_.resize(tool_requests_.size());

  size_t total_fields = 0;
  for (size_t request_idx = 0; request_idx < tool_requests_.size();
       ++request_idx) {
    const FormFillingRequest& tool_request = tool_requests_[request_idx];
    CHECK(!tool_request.trigger_fields.empty());
    size_t fields_size = tool_request.trigger_fields.size();

    service_requests_[request_idx].requested_data = tool_request.requested_data;
    service_requests_[request_idx].section_label = tool_request.section_label;
    service_requests_[request_idx].trigger_fields.resize(fields_size);

    total_fields += fields_size;
  }

  // Resolve target frame for each trigger field.
  // Invoke `PopulateAutofillRendererIds` when all requests are filled.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      total_fields,
      base::BindOnce(&AttemptFormFillingTool::PopulateAutofillRendererIds,
                     weak_ptr_factory_.GetWeakPtr()));

  for (size_t request_idx = 0; request_idx < tool_requests_.size();
       ++request_idx) {
    for (size_t field_idx = 0;
         field_idx < tool_requests_[request_idx].trigger_fields.size();
         ++field_idx) {
      const ActionTarget& target =
          tool_requests_[request_idx].trigger_fields[field_idx];
      ResolveTargetFrame(
          web_state_, frames_manager->GetMainWebFrame()->AsWeakPtr(), target,
          base::BindOnce(&AttemptFormFillingTool::OnTargetFrameResolved,
                         weak_ptr_factory_.GetWeakPtr(), request_idx, field_idx,
                         barrier_closure));
    }
  }
}

autofill::AutofillClientIOS& AttemptFormFillingTool::GetAutofillClient() const {
  CHECK(web_state_);
  autofill::AutofillClientIOS* client =
      autofill::AutofillClientIOS::FromWebState(web_state_.get());
  CHECK(client);
  return *client;
}

void AttemptFormFillingTool::OnTargetFrameResolved(
    size_t request_idx,
    size_t field_idx,
    base::RepeatingClosure barrier_closure,
    base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                   ToolExecutionResult> result) {
  if (!result.has_value()) {
    FailWithResult(result.error());
    return;
  }

  ActionTargetJavaScriptFeature::TargetFrameResult target_frame =
      result.value();
  web::WebFrame* target_web_frame = target_frame.frame;
  if (!target_web_frame) {
    FailWithResult(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  tool_requests_[request_idx].trigger_fields[field_idx] = target_frame.target;
  frame_to_indices_map_[target_web_frame->GetFrameId()].push_back(
      {request_idx, field_idx});

  // Run the barrier closure to signal this request is resolved.
  barrier_closure.Run();
}

void AttemptFormFillingTool::PopulateAutofillRendererIds() {
  if (!web_state_) {
    FailWithResult(ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  web::WebFramesManager* frames_manager =
      ActionTargetJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_.get());
  if (!frames_manager) {
    FailWithResult(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }

  // Call `OnAllAutofillRendererIdsRetrieved` when all autofill renderer IDs are
  // retrieved.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      frame_to_indices_map_.size(),
      base::BindOnce(&AttemptFormFillingTool::OnAllAutofillRendererIdsRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));

  // For each web frame in `frame_to_indices_map_`, request autofill renderer
  // IDs for the trigger fields in it.
  for (const auto& [frame_id, indices] : frame_to_indices_map_) {
    web::WebFrame* frame = frames_manager->GetFrameWithId(frame_id);
    if (!frame) {
      FailWithResult(
          ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
      return;
    }

    std::vector<ActionTarget> fields;
    fields.reserve(indices.size());
    for (const auto& [request_idx, field_idx] : indices) {
      fields.push_back(
          std::move(tool_requests_[request_idx].trigger_fields[field_idx]));
    }

    AttemptFormFillingToolJavaScriptFeature::GetInstance()
        ->GetAutofillRendererIds(
            frame->AsWeakPtr(), std::move(fields),
            base::BindOnce(
                &AttemptFormFillingTool::OnRequestRendererIdsResolved,
                weak_ptr_factory_.GetWeakPtr(), frame->AsWeakPtr(),
                barrier_closure));
  }
}

void AttemptFormFillingTool::OnRequestRendererIdsResolved(
    base::WeakPtr<web::WebFrame> web_frame,
    base::RepeatingClosure barrier_closure,
    base::expected<std::vector<uint32_t>, ToolExecutionResult> result) {
  if (!web_frame) {
    FailWithResult(
        ToolExecutionResult(mojom::ActionResultCode::kFrameWentAway));
    return;
  }
  if (!result.has_value()) {
    FailWithResult(result.error());
    return;
  }

  const std::string frame_id = web_frame->GetFrameId();

  // Deserialize target frame's LocalFrameToken.
  std::optional<base::UnguessableToken> unguessable_token =
      autofill::DeserializeJavaScriptFrameId(frame_id);
  if (!unguessable_token) {
    FailWithResult(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }
  autofill::LocalFrameToken frame_token(*unguessable_token);

  // Save renderer ID in `service_requests_`.
  std::vector<uint32_t> renderer_ids = std::move(result.value());
  std::vector<std::pair<size_t, size_t>> indices =
      frame_to_indices_map_[frame_id];
  if (renderer_ids.size() != indices.size()) {
    FailWithResult(ToolExecutionResult(
        mojom::ActionResultCode::kArgumentsInvalid,
        /*requires_page_stabilization=*/false,
        "Wrong number of autofill renderer IDs returned from JavaScript."));
    return;
  }

  for (size_t idx = 0; idx < renderer_ids.size(); ++idx) {
    auto [request_idx, field_idx] = indices[idx];
    autofill::ActorFormFillingService::FillRequest& service_request =
        service_requests_[request_idx];
    CHECK(!service_request.trigger_fields[field_idx]);
    CHECK_NE(renderer_ids[idx], 0u);
    autofill::FieldRendererId renderer_id(renderer_ids[idx]);
    service_request.trigger_fields[field_idx] =
        autofill::FieldGlobalId{frame_token, renderer_id};
  }

  // Run the barrier closure to signal this frame's requests are resolved.
  barrier_closure.Run();
}

void AttemptFormFillingTool::OnAllAutofillRendererIdsRetrieved() {
  if (!web_state_) {
    FailWithResult(ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // Run a final validation before passing the requests to
  // `ActorFormFillingService`.
  CHECK(!service_requests_.empty());
  for (const autofill::ActorFormFillingService::FillRequest& service_request :
       service_requests_) {
    const std::vector<autofill::FieldGlobalId>& fields =
        service_request.trigger_fields;
    CHECK(!fields.empty());
    for (const autofill::FieldGlobalId& field : fields) {
      CHECK(!field.frame_token.is_empty());
      CHECK_NE(field.renderer_id, autofill::FieldRendererId());
    }
  }

  // Passing the requests to the service.
  // TODO(crbug.com/472287741): Explore pre-click form. See
  // `features::kGlicActorAutofillPreClick`.
  ActorTaskFormFillingHandler* form_filling_handler =
      tool_delegate_->GetActorTaskFormFillingHandler();
  CHECK(form_filling_handler);
  autofill::ActorFormFillingService* form_filling_service =
      form_filling_handler->GetActorFormFillingService();
  CHECK(form_filling_service);

  form_filling_service->GetSuggestions(
      GetAutofillClient(), std::move(service_requests_),
      base::BindOnce(&AttemptFormFillingTool::OnSuggestionsRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AttemptFormFillingTool::OnSuggestionsRetrieved(
    base::expected<std::vector<autofill::ActorFormFillingRequest>,
                   autofill::ActorFormFillingError> suggestions_result) {
  if (!web_state_) {
    FailWithResult(ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  if (!suggestions_result.has_value()) {
    FailWithResult(FromServiceError(suggestions_result.error()));
    return;
  }

  std::vector<autofill::ActorFormFillingRequest> suggestions_result_values =
      std::move(suggestions_result.value());
  if (suggestions_result_values.empty()) {
    FailWithResult(ToolExecutionResult(
        mojom::ActionResultCode::kFormFillingNoSuggestionsAvailable));
    return;
  }

  selected_suggestions_.resize(suggestions_result_values.size());

  ActorTaskFormFillingHandler* form_filling_handler =
      tool_delegate_->GetActorTaskFormFillingHandler();
  CHECK(form_filling_handler);
  form_filling_handler->RegisterAutofillSuggestionsAndCallback(
      std::move(suggestions_result_values),
      base::BindRepeating(&AttemptFormFillingTool::OnSuggestionSelected,
                          weak_ptr_factory_.GetWeakPtr()));

  current_request_index_ = 0;
  ScrollToCurrentRequestAndShowSuggestions();
}

void AttemptFormFillingTool::ScrollToCurrentRequestAndShowSuggestions() {
  CHECK(web_state_);
  CHECK_LT(current_request_index_, selected_suggestions_.size());

  ActorTaskFormFillingHandler* form_filling_handler =
      tool_delegate_->GetActorTaskFormFillingHandler();
  CHECK(form_filling_handler);
  form_filling_handler->GetActorFormFillingService()->ScrollToForm(
      GetAutofillClient(), current_request_index_);
  form_filling_handler->PromptToSelectAutofillSuggestion(
      current_request_index_);
}

void AttemptFormFillingTool::OnSuggestionSelected(
    size_t index,
    base::expected<std::optional<autofill::ActorSuggestion>,
                   ToolExecutionResult> selected_suggestion) {
  if (!web_state_) {
    FailWithResult(ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }
  if (!selected_suggestion.has_value()) {
    FailWithResult(selected_suggestion.error());
    return;
  }
  if (!selected_suggestion.value().has_value()) {
    FailWithResult(ToolExecutionResult(
        mojom::ActionResultCode::kFormFillingDialogError,
        /*requires_page_stabilization=*/false,
        "Dialog response contains no selected suggestions."));
    return;
  }

  // Forms should be shown sequentially.
  CHECK_EQ(index, current_request_index_);

  ActorTaskFormFillingHandler* form_filling_handler =
      tool_delegate_->GetActorTaskFormFillingHandler();
  CHECK(form_filling_handler);
  autofill::ActorFormFillingService* service =
      form_filling_handler->GetActorFormFillingService();
  CHECK(service);
  autofill::AutofillClientIOS& client = GetAutofillClient();
  autofill::ActorSuggestionId suggestion_id = selected_suggestion.value()->id;

  service->FillForm(client, index,
                    autofill::ActorFormFillingSelection(suggestion_id));
  selected_suggestions_[index].selected_suggestion_id = suggestion_id;

  // If there are remaining requests, proceed with the next form in order.
  if (++current_request_index_ < selected_suggestions_.size()) {
    ScrollToCurrentRequestAndShowSuggestions();
    return;
  }

  service->FillSuggestions(
      client, std::move(selected_suggestions_),
      base::BindOnce(&AttemptFormFillingTool::OnFormFillingComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AttemptFormFillingTool::OnFormFillingComplete(
    base::expected<std::string, autofill::ActorFormFillingError> result) {
  if (!result.has_value()) {
    FailWithResult(FromServiceError(result.error()));
    return;
  }
  std::move(execute_callback_)
      .Run(ToolExecutionResult(mojom::ActionResultCode::kOk,
                               /*requires_page_stabilization=*/true,
                               result.value()));
}

void AttemptFormFillingTool::FailWithResult(ToolExecutionResult result) {
  if (!execute_callback_) {
    return;
  }
  CHECK(!result.IsOk());
  // Stops subsequent callbacks from running after `execute_callback_` runs.
  weak_ptr_factory_.InvalidateWeakPtrs();
  std::move(execute_callback_).Run(result);
}

base::WeakPtr<web::WebState> AttemptFormFillingTool::GetTargetWebState() const {
  return web_state_;
}

ToolType AttemptFormFillingTool::GetToolType() const {
  return ToolType::kAttemptFormFilling;
}

}  // namespace actor
