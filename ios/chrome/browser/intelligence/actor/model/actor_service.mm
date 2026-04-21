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

namespace {

// Logs a failure to create a tool to the journal.
void LogToolCreationFailed(AggregatedJournal* journal,
                           ActorTaskId task_id,
                           const std::string& tool_name,
                           const ActorToolError& error) {
  CHECK(journal);

  std::vector<JournalDetails> details = {
      {"error", GetActorToolErrorMessage(error)}};

  journal->Log(
      GURL(), task_id,
      base::StringPrintf("Failed to create tool: %s", tool_name.c_str()),
      std::move(details));
}

// Logs an attempt to create a tool to the journal.
void LogToolCreationAttempt(AggregatedJournal* journal,
                            ActorTaskId task_id,
                            const std::string& tool_name) {
  CHECK(journal);

  journal->Log(
      GURL(), task_id,
      base::StringPrintf("Attempting to create tool: %s", tool_name.c_str()),
      std::vector<JournalDetails>());
}

}  // namespace

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

ActorTaskId ActorService::CreateTask(const std::string& title,
                                     bool allow_incognito_web_states) {
  CHECK(IsActorEnabled());

  const ActorTaskId task_id = next_task_id_.GenerateNextId();
  active_tasks_[task_id] =
      std::make_unique<ActorTask>(task_id, title, journal_.get());
  return task_id;
}

CreateActorToolsResult ActorService::CreateActorTools(
    const std::vector<optimization_guide::proto::Action>& actions,
    ActorTaskId task_id) {
  CHECK(IsActorEnabled());

  std::vector<std::unique_ptr<ActorTool>> tools;
  tools.reserve(actions.size());

  for (const auto& action : actions) {
    std::string tool_name = ActorActionCaseToToolName(action.action_case())
                                .value_or("unknown tool");

    LogToolCreationAttempt(journal_.get(), task_id, tool_name);

    if (action.action_case() ==
        optimization_guide::proto::Action::ACTION_NOT_SET) {
      ActorToolError error{ActorToolErrorCode::kUnsupportedAction};
      LogToolCreationFailed(journal_.get(), task_id, tool_name, error);
      return base::unexpected(error);
    }

    if (IsToolDisabled(action.action_case())) {
      ActorToolError error{ActorToolErrorCode::kToolDisabledByFeature};
      LogToolCreationFailed(journal_.get(), task_id, tool_name, error);
      return base::unexpected(error);
    }

    base::expected<std::unique_ptr<ActorTool>, ActorToolError>
        create_tool_result = tool_factory_->CreateTool(action, profile_);

    if (!create_tool_result.has_value()) {
      LogToolCreationFailed(journal_.get(), task_id, tool_name,
                            create_tool_result.error());
      return base::unexpected(create_tool_result.error());
    }

    tools.push_back(std::move(create_tool_result.value()));
  }

  return tools;
}

void ActorService::PerformActions(
    ActorTaskId task_id,
    std::vector<std::unique_ptr<ActorTool>> actions,
    const std::string& task_update,
    PerformActionsCallback callback) {
  CHECK(IsActorEnabled());

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
