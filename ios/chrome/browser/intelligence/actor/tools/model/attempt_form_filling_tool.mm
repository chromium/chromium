// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool.h"

#import "base/barrier_closure.h"
#import "base/functional/bind.h"
#import "base/notimplemented.h"
#import "base/strings/string_number_conversions.h"
#import "base/types/expected.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// Enum type conversion for RequestedData.
actor::FormFillingRequest::RequestedData MapRequestedData(
    optimization_guide::proto::FormFillingRequest_RequestedData proto_enum) {
  using RequestedData = actor::FormFillingRequest::RequestedData;
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

  CHECK(!tool_requests_.empty());

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

  tool_requests_.clear();
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

  // Deserialize target frame's LocalFrameToken.
  std::optional<base::UnguessableToken> unguessable_token =
      autofill::DeserializeJavaScriptFrameId(web_frame->GetFrameId());
  if (!unguessable_token) {
    FailWithResult(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }
  autofill::LocalFrameToken frame_token(*unguessable_token);

  // Save renderer ID in `service_requests_`.
  std::vector<uint32_t> renderer_ids = std::move(result.value());
  std::vector<std::pair<size_t, size_t>> indices =
      frame_to_indices_map_[web_frame->GetFrameId()];
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
  // TODO(crbug.com/472287741): Call `GetSuggestions` on
  // `ActorFormFillingService` and handle selections.
  std::move(execute_callback_).Run(ToolExecutionResult::Ok());
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
