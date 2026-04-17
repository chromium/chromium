// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"
#import "ios/chrome/browser/intelligence/actor/model/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"

namespace actor {

ActorService::ActorService(ProfileIOS* profile)
    : ActorService(profile, std::make_unique<ActorToolFactory>()) {}

ActorService::ActorService(ProfileIOS* profile,
                           std::unique_ptr<ActorToolFactory> tool_factory)
    : profile_(profile),
      tool_factory_(std::move(tool_factory)),
      journal_(std::make_unique<AggregatedJournal>()) {
  CHECK(tool_factory_);
}

ActorService::~ActorService() = default;

void ActorService::Shutdown() {}

void ActorService::ExecuteAction(
    const optimization_guide::proto::Action& action,
    ToolExecutionCallback callback) {
  CHECK(IsActorEnabled());

  if (action.action_case() ==
      optimization_guide::proto::Action::ACTION_NOT_SET) {
    std::move(callback).Run(base::unexpected(
        ActorToolError{ActorToolErrorCode::kUnsupportedAction}));
    return;
  }

  if (IsToolDisabled(action.action_case())) {
    std::move(callback).Run(base::unexpected(
        ActorToolError{ActorToolErrorCode::kToolDisabledByFeature}));
    return;
  }

  std::string tool_name =
      ActorActionCaseToToolName(action.action_case()).value_or("unknown tool");

  // Log immediate attempt to create tool.
  journal_->Log(
      GURL(), ActorTaskId(),
      base::StringPrintf("Attempting to create tool: %s", tool_name.c_str()),
      {});

  base::expected<std::unique_ptr<ActorTool>, ActorToolError>
      create_tool_result = tool_factory_->CreateTool(action, profile_);

  if (!create_tool_result.has_value()) {
    // Log immediate failure to create tool.
    journal_->Log(
        GURL(), ActorTaskId(),
        base::StringPrintf("Failed to create tool: %s", tool_name.c_str()),
        {{"error", base::NumberToString(
                       static_cast<int>(create_tool_result.error().code))}});
    std::move(callback).Run(base::unexpected(create_tool_result.error()));
    return;
  }

  // The `tool` is moved into the callback to ensure it stays alive until
  // the async operation completes.
  std::unique_ptr<ActorTool> tool = std::move(create_tool_result.value());
  ActorTool* tool_ptr = tool.get();

  // Start a Begin log when the Execute call starts.
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> entry =
      journal_->CreatePendingAsyncEntry(
          GURL(), ActorTaskId(), 0,
          base::StringPrintf("Execute Tool: %s", tool_name.c_str()), {});

  ToolExecutionCallback wrapped_callback = base::BindOnce(
      [](std::unique_ptr<ActorTool> tool,
         std::unique_ptr<AggregatedJournal::PendingAsyncEntry> entry,
         ToolExecutionCallback callback, ToolExecutionResult result) {
        std::vector<JournalDetails> details;
        if (!result.has_value()) {
          // Log if an error happens between Begin and End.
          details.push_back({"error", base::NumberToString(static_cast<int>(
                                          result.error().code))});
        }
        // End log when the Execute call finishes.
        entry->EndEntry(std::move(details));

        std::move(callback).Run(std::move(result));

        // `tool` is destroyed here when the lambda finishes.
        //
        // The lifetime of `tool` will be better managed once we set up the
        // orchestration layer.
      },
      std::move(tool), std::move(entry), std::move(callback));

  tool_ptr->Execute(std::move(wrapped_callback));
}

ActorTaskId ActorService::CreateTask(const std::string& title,
                                     bool allow_incognito_web_states) {
  const ActorTaskId task_id = next_task_id_.GenerateNextId();
  active_tasks_[task_id] = std::make_unique<ActorTask>(task_id, title);
  return task_id;
}

void ActorService::PerformActions(
    ActorTaskId task_id,
    std::vector<std::unique_ptr<ActorTool>> actions,
    const std::string& task_update,
    PerformActionsCallback callback) {
  auto it = active_tasks_.find(task_id);
  if (it == active_tasks_.end()) {
    // TODO(crbug.com/503054406): Return high level error for non-existent
    // task.
    std::move(callback).Run(std::vector<ActionResult>());
    return;
  }

  // TODO(crbug.com/503054406): Return high level error for already acting task.
  it->second->Act(std::move(actions), task_update, std::move(callback));
}

void ActorService::RequestTabObservation(ActorTaskId task_id,
                                         web::WebState* web_state,
                                         TabObservationCallback callback) {
  auto it = active_tasks_.find(task_id);
  if (it == active_tasks_.end() || !web_state) {
    std::move(callback).Run(PageContextWrapperCallbackResponse());
    return;
  }

  PageContextWrapperConfigBuilder builder;
  builder.SetUseRichExtraction(true);
  builder.SetUseRichExtractionWithActionable(true);
  PageContextWrapperConfig config = builder.Build();

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  PageContextWrapper* page_context_wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state
                  config:config
      completionCallback:base::BindOnce(
                             &ActorService::OnPageContextExtractionComplete,
                             weak_ptr_factory_.GetWeakPtr(), web_state_id,
                             std::move(callback))];

  pending_observations_[web_state_id] = page_context_wrapper;

  [page_context_wrapper populatePageContextFieldsAsync];
}

void ActorService::OnPageContextExtractionComplete(
    web::WebStateID web_state_id,
    TabObservationCallback callback,
    PageContextWrapperCallbackResponse response) {
  pending_observations_.erase(web_state_id);
  std::move(callback).Run(std::move(response));
}

void ActorService::PauseTask(ActorTaskId task_id, bool from_actor) {
  // TODO(crbug.com/496163986): Implement and test.
}

void ActorService::StopTask(ActorTaskId task_id,
                            ActorTaskStoppedReason reason) {
  // TODO(crbug.com/496163986): Implement and test.
  active_tasks_.erase(task_id);
}

std::vector<optimization_guide::proto::Action::ActionCase>
ActorService::GetSupportedCapabilities() const {
  return tool_factory_->GetSupportedCapabilities();
}

}  // namespace actor
