// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"

#import <algorithm>

#import "base/barrier_callback.h"
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
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/web/public/web_state.h"

namespace actor {

namespace {

// Logs a failure to create a tool to the journal.
void LogToolCreationFailed(AggregatedJournal* journal,
                           ActorTaskId task_id,
                           const std::string& tool_name,
                           const ToolExecutionResult& error) {
  CHECK(journal);

  std::vector<JournalDetails> details = {
      {"error", GetToolExecutionResultMessage(error)}};

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
  active_tasks_[task_id] = std::make_unique<ActorTask>(
      task_id, title, allow_incognito_web_states, journal_.get());
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
      ToolExecutionResult error{InternalToolErrorCode::kUnsupportedAction};
      LogToolCreationFailed(journal_.get(), task_id, tool_name, error);
      return base::unexpected(error);
    }

    if (IsToolDisabled(action.action_case())) {
      ToolExecutionResult error{InternalToolErrorCode::kToolDisabledByFeature};
      LogToolCreationFailed(journal_.get(), task_id, tool_name, error);
      return base::unexpected(error);
    }

    base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult>
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
    PerformActionsResult actions_result;
    std::move(callback).Run(std::move(actions_result));
    return;
  }

  it->second->Act(std::move(actions), task_update,
                  base::BindOnce(&ActorService::OnActCompleted,
                                 weak_ptr_factory_.GetWeakPtr(), task_id,
                                 std::move(callback)));
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

void ActorService::OnActCompleted(ActorTaskId task_id,
                                  PerformActionsCallback callback,
                                  std::vector<ActionResult> results) {
  PerformActionsResult perform_actions_result;
  perform_actions_result.action_results = std::move(results);

  auto it = active_tasks_.find(task_id);
  if (it == active_tasks_.end()) {
    std::move(callback).Run(std::move(perform_actions_result));
    return;
  }

  ActorTask* task = it->second.get();
  std::vector<web::WebState*> web_states_to_extract;

  // TODO(crbug.com/505080093): Extract PageContext for *all* of the controlled
  // WebStates when this is supported, instead of just one. Right now there
  // should only be one, we iterate until the first valid one.
  for (const auto& weak_web_state : task->controlled_web_states()) {
    if (web::WebState* web_state = weak_web_state.get()) {
      web_states_to_extract.push_back(web_state);
      break;
    }
  }

  if (web_states_to_extract.empty()) {
    std::move(callback).Run(std::move(perform_actions_result));
    return;
  }

  // Barrier to wait for all PageContext extractions and add them to
  // ActionsResult.
  auto barrier = base::BarrierCallback<std::unique_ptr<TabObservationResponse>>(
      web_states_to_extract.size(),
      base::BindOnce(
          [](PerformActionsCallback callback, PerformActionsResult result,
             std::vector<std::unique_ptr<TabObservationResponse>> contexts) {
            result.page_contexts = std::move(contexts);
            std::move(callback).Run(std::move(result));
          },
          std::move(callback), std::move(perform_actions_result)));

  for (web::WebState* web_state : web_states_to_extract) {
    web::WebStateID id = web_state->GetUniqueIdentifier();
    RequestTabObservation(
        task_id, web_state,
        base::BindOnce(
            [](web::WebStateID id,
               base::RepeatingCallback<void(
                   std::unique_ptr<actor::TabObservationResponse>)> barrier,
               PageContextWrapperCallbackResponse response) {
              barrier.Run(std::make_unique<actor::TabObservationResponse>(
                  id, std::move(response), true));
            },
            id, barrier));
  }
}

web::WebState* ActorService::GetWebStateForID(web::WebStateID web_state_id,
                                              ActorTaskId task_id) {
  bool allows_incognito = false;
  auto it = active_tasks_.find(task_id);
  if (it != active_tasks_.end()) {
    allows_incognito = it->second->allow_incognito_web_states();
  } else {
    return nullptr;
  }

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);

  std::set<Browser*> browsers =
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);
  if (allows_incognito) {
    const std::set<Browser*>& incognito_browsers =
        browser_list->BrowsersOfType(BrowserList::BrowserType::kIncognito);
    browsers.insert(incognito_browsers.begin(), incognito_browsers.end());
  }

  BrowserAndIndex browser_and_index =
      FindBrowserAndIndex(web_state_id, browsers);

  if (browser_and_index.tab_index == WebStateList::kInvalidIndex ||
      !browser_and_index.browser) {
    return nullptr;
  }

  web::WebState* web_state =
      browser_and_index.browser->GetWebStateList()->GetWebStateAt(
          browser_and_index.tab_index);

  if (!web_state) {
    return nullptr;
  }

  // Check if the WebState is in the task's controlled WebStates.
  if (!std::ranges::contains(it->second->controlled_web_states(), web_state,
                             &base::WeakPtr<web::WebState>::get)) {
    return nullptr;
  }

  return web_state;
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
