// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_FORM_FILLING_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_FORM_FILLING_TOOL_H_

#import <map>
#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "components/autofill/core/browser/actor/actor_form_filling_service.h"
#import "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/web_actor_tool.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {
class AutofillClientIOS;
}  // namespace autofill

namespace actor {

class ActionTarget;
class ToolDelegate;

// A struct representing the request to attempt form filling.
struct FormFillingRequest {
  FormFillingRequest();
  ~FormFillingRequest();
  FormFillingRequest(const FormFillingRequest&);
  FormFillingRequest& operator=(const FormFillingRequest&);
  FormFillingRequest(FormFillingRequest&&);
  FormFillingRequest& operator=(FormFillingRequest&&);

  autofill::ActorFormFillingRequestedData requested_data{};
  std::string section_label;
  std::vector<ActionTarget> trigger_fields;
};

// Tool to attempt form filling on iOS.
class AttemptFormFillingTool : public WebActorTool {
 public:
  // Creates an instance of AttemptFormFillingTool. Returns nullptr if the
  // action cannot be converted to a valid set of FormFillingRequests.
  static std::unique_ptr<AttemptFormFillingTool> Create(
      base::WeakPtr<web::WebState> web_state,
      const optimization_guide::proto::AttemptFormFillingAction& action,
      ToolDelegate* tool_delegate);

  ~AttemptFormFillingTool() override;

  // ActorTool:
  void Validate(ToolExecutionCallback callback) override;
  void Execute(ToolExecutionCallback callback) override;
  base::WeakPtr<web::WebState> GetTargetWebState() const override;
  ToolType GetToolType() const override;

 private:
  AttemptFormFillingTool(base::WeakPtr<web::WebState> web_state,
                         std::vector<FormFillingRequest> requests,
                         ToolDelegate* tool_delegate);

  // Returns the AutofillClientIOS associated with the target WebState.
  autofill::AutofillClientIOS& GetAutofillClient() const;

  // Callback invoked when the target frame resolution for a request completes.
  void OnTargetFrameResolved(
      size_t request_idx,
      size_t field_idx,
      base::RepeatingClosure barrier_closure,
      base::expected<ActionTargetJavaScriptFeature::TargetFrameResult,
                     ToolExecutionResult> result);

  // Retrieve autofill renderer IDs for all targets.
  void PopulateAutofillRendererIds();

  // Callback invoked when the autofill renderer IDs for a frame are retrieved.
  void OnRequestRendererIdsResolved(
      base::WeakPtr<web::WebFrame> web_frame,
      base::RepeatingClosure barrier_closure,
      base::expected<std::vector<uint32_t>, ToolExecutionResult> result);

  // Callback invoked when all autofill renderer IDs are retrieved.
  void OnAllAutofillRendererIdsRetrieved();

  // Callback invoked when the form filling service returns suggestions.
  void OnSuggestionsRetrieved(
      base::expected<std::vector<autofill::ActorFormFillingRequest>,
                     autofill::ActorFormFillingError> suggestions_result);

  // Scrolls the web page to the form corresponding to the current request and
  // prompts the user to select a suggestion.
  void ScrollToCurrentRequestAndShowSuggestions();

  // Callback invoked when the user has selected a suggestion or dismissed the
  // suggestion selection interface.
  void OnSuggestionSelected(
      size_t index,
      base::expected<std::optional<autofill::ActorSuggestion>,
                     ToolExecutionResult> selected_suggestion);

  // Invoked when the form filling service has completed filling suggestions.
  void OnFormFillingComplete(
      base::expected<std::string, autofill::ActorFormFillingError> result);

  // Helper method to handle execution failure. Runs the execution callback
  // with the error result and invalidates all pending callbacks.
  void FailWithResult(ToolExecutionResult result);

  // The tab this tool is executing on.
  base::WeakPtr<web::WebState> web_state_;

  // The list of requests containing requested data type and raw ActionTarget
  // trigger fields.
  std::vector<FormFillingRequest> tool_requests_;

  // Delegate handling UI interactions.
  raw_ptr<ToolDelegate> tool_delegate_ = nullptr;

  // The callback to run when tool execution completes or fails.
  ToolExecutionCallback execute_callback_;

  // The list of requests to be sent to `ActorFormFillingService` to retrieve
  // suggestions.
  std::vector<autofill::ActorFormFillingService::FillRequest> service_requests_;

  // Maps resolved WebFrame's stable ID to the request and field indices
  // belonging to it. Reduces IPC calls to retrieve autofill field IDs for each
  // trigger field in the request.
  std::map<std::string, std::vector<std::pair<size_t, size_t>>>
      frame_to_indices_map_;

  // The list of suggestions selected by the user.
  std::vector<autofill::ActorFormFillingSelection> selected_suggestions_;

  // The index of the request currently being processed and presented.
  size_t current_request_index_ = 0;

  base::WeakPtrFactory<AttemptFormFillingTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ATTEMPT_FORM_FILLING_TOOL_H_
