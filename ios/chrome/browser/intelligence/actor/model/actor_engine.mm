// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/strings/stringprintf.h"
#import "components/actor/core/safety_list_manager.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "components/origin_gating/core/origin_gating_configuration.h"
#import "components/origin_gating/core/origin_gating_registration.h"
#import "components/origin_gating/core/origin_gating_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task_intervention_handler.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_task_form_filling_handler.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_controller.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/logging_util.h"

namespace actor {

namespace {

// Evaluates the destination against the Actor Safety List component data.
origin_gating::Decision EvaluateSafetyListPredicate(
    origin_gating::GatingDecisionContext* context,
    const GURL& source,
    const GURL& destination) {
  actor::SafetyListManager* safety_list_manager =
      actor::SafetyListManager::GetInstance();
  if (!safety_list_manager) {
    return origin_gating::Decision::kNoDecision;
  }

  const GURL& effective_source = source.is_empty() ? destination : source;
  switch (safety_list_manager->Find(effective_source, destination)) {
    case actor::SafetyListManager::Decision::kAllow:
      return origin_gating::Decision::kAllowed;
    case actor::SafetyListManager::Decision::kBlock:
      return origin_gating::Decision::kBlocked;
    case actor::SafetyListManager::Decision::kNone:
      return origin_gating::Decision::kNoDecision;
  }
}

// Returns the string representation of the ActorEngine::State.
std::string ActorEngineStateToString(ActorEngine::State state) {
  switch (state) {
    case ActorEngine::State::kUnknown:
      return "Unknown";
    case ActorEngine::State::kInit:
      return "Init";
    case ActorEngine::State::kPreExecutionChecks:
      return "PreExecutionChecks";
    case ActorEngine::State::kToolVerify:
      return "ToolVerify";
    case ActorEngine::State::kUiPreInvoke:
      return "UiPreInvoke";
    case ActorEngine::State::kToolInvoke:
      return "ToolInvoke";
    case ActorEngine::State::kUiPostInvoke:
      return "UiPostInvoke";
    case ActorEngine::State::kCompleted:
      return "Completed";
    case ActorEngine::State::kFailed:
      return "Failed";
  }
}

// Returns the string representation of the ActorEngine::EngineResult.
std::string EngineResultToString(ActorEngine::EngineResult result) {
  switch (result) {
    case ActorEngine::EngineResult::kUnknown:
      return "Unknown";
    case ActorEngine::EngineResult::kSuccess:
      return "Success";
    case ActorEngine::EngineResult::kFailed:
      return "Failed";
    case ActorEngine::EngineResult::kTimeout:
      return "Timeout";
    case ActorEngine::EngineResult::kCancelled:
      return "Cancelled";
  }
}

// TODO(crbug.com/503841160): Log the proper WebState URLs.
// Logs the start of the Act sequence to the journal.
void LogActStart(
    AggregatedJournal& journal,
    ActorTaskId task_id,
    const std::vector<std::unique_ptr<ActorToolRequest>>& actions) {
  std::vector<std::pair<std::string, std::string>> details;
  for (size_t i = 0; i < actions.size(); ++i) {
    details.push_back({base::StringPrintf("Actions[%zu]", i),
                       base::StringPrintf("Tool %zu", i)});
  }
  LogJournalEvent(journal, GURL(), task_id, "ExecutionEngine::Act", details);
}
// Returns the WebStateID for the target WebState of `action`, or an invalid
// WebStateID if `action` is null or has no target WebState.
web::WebStateID GetWebStateIDForAction(const ActorToolRequest* action) {
  if (!action) {
    return web::WebStateID();
  }
  return action->GetTargetWebStateId();
}

}  // namespace

ActorEngine::ActorEngine(ExecutionUpdatesDelegate* execution_updates_delegate,
                         ActorTask* owner_task)
    : state_(State::kInit),
      execution_updates_delegate_(execution_updates_delegate),
      owner_task_(owner_task) {
  CHECK(execution_updates_delegate_);
  CHECK(owner_task_);
  origin_gating::OriginGatingService* origin_gating_service =
      owner_task_->GetToolFactory()
          .profile_context_resolver()
          .GetOriginGatingService();
  CHECK(origin_gating_service);
  origin_gating_registration_ = origin_gating_service->CreateAndRegisterChecker(
      origin_gating_delegate_.GetWeakPtr(), CreateOriginGatingConfig());
  CHECK(origin_gating_registration_);
}

ActorEngine::~ActorEngine() = default;

void ActorEngine::Act(std::vector<std::unique_ptr<ActorToolRequest>> actions,
                      ActCallback callback) {
  // TODO(crbug.com/503054406): Add guards for invalid start states.
  action_sequence_ = std::move(actions);
  completion_callback_ = std::move(callback);
  next_action_index_ = 0;
  action_results_.clear();

  LogActStart(GetJournal(), GetTaskId(), action_sequence_);

  ExecuteNextAction();
}

void ActorEngine::CancelOngoingAndPendingActions(
    ActorEngine::EngineResult reason) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  action_sequence_.clear();

  if (tool_controller_) {
    tool_controller_->Cancel();
    tool_controller_.reset();
  }

  SetState(State::kFailed);

  LogJournalEvent(GetJournal(), GURL(), GetTaskId(), "ExecutionEngine::Cancel",
                  {{"reason", EngineResultToString(reason)}});

  if (completion_callback_) {
    std::move(completion_callback_).Run(std::move(action_results_));
  }
}

void ActorEngine::FailCurrentTool(mojom::ActionResultCode reason) {
  if (state_ != State::kToolInvoke || !tool_controller_) {
    return;
  }

  tool_controller_->FailCurrentTool(reason);
}

void ActorEngine::PauseOngoingActions() {
  // TODO(crbug.com/548051839): Support pausing in-progress tool actions.
}

void ActorEngine::DidUninterruptTask() {
  // TODO(crbug.com/548051839): Support resuming deferred tool invoke.
}

#pragma mark - ToolDelegate

ActorTaskId ActorEngine::GetTaskId() const {
  CHECK(owner_task_);
  return owner_task_->task_id();
}

AggregatedJournal& ActorEngine::GetJournal() const {
  CHECK(owner_task_);
  return owner_task_->GetJournal();
}

ActorToolFactory& ActorEngine::GetToolFactory() const {
  CHECK(owner_task_);
  return owner_task_->GetToolFactory();
}

ActorTaskFormFillingHandler* ActorEngine::GetActorTaskFormFillingHandler() {
  if (!form_filling_handler_) {
    intervention_handler_ = [[ActorTaskInterventionHandler alloc] init];
    form_filling_handler_ = ActorTaskFormFillingHandler::Create(
        base::PassKey<ActorEngine>(), GetJournal(), GetTaskId());
    form_filling_handler_->SetInterventionDelegate(base::PassKey<ActorEngine>(),
                                                   intervention_handler_);
  }
  return form_filling_handler_.get();
}

void ActorEngine::InterruptFromTool() {
  CHECK(owner_task_);
  // TODO(crbug.com/566215654): Add an explicit `ActorTaskInterruptReason` for
  // tool interrupts.
  owner_task_->Interrupt(ActorTaskInterruptReason::kUnknownReason);
}

void ActorEngine::UninterruptFromTool() {
  CHECK(owner_task_);
  owner_task_->Uninterrupt(ActorTaskState::kActing);
}

bool ActorEngine::IsWindowIdValid(int32_t window_id) {
  CHECK(owner_task_);
  return owner_task_->IsWindowIdValid(window_id);
}

web::WebState* ActorEngine::InsertWebState(
    int32_t window_id,
    const web::NavigationManager::WebLoadParams& load_params,
    bool in_background) {
  CHECK(owner_task_);
  return owner_task_->InsertWebState(window_id, load_params, in_background);
}

origin_gating::CheckerId ActorEngine::GetOriginGatingCheckerId() const {
  return origin_gating_registration_->id();
}

origin_gating::OriginGatingChecker* ActorEngine::GetOriginGatingChecker() {
  return origin_gating_registration_->service().GetChecker(
      origin_gating_registration_->id());
}

#pragma mark - Private

// static
origin_gating::OriginGatingConfiguration
ActorEngine::CreateOriginGatingConfig() {
  return origin_gating::OriginGatingConfiguration(
      /*predicates=*/
      {
          origin_gating::PredicateConfiguration(
              /*predicate=*/origin_gating::CustomPredicate(
                  base::BindRepeating(&EvaluateSafetyListPredicate),
                  ActorCustomPredicate::kSafetyList),
              /*events=*/
              {// Gate explicit navigation requests to prevent the actor from
               // navigating to unapproved or dangerous destinations.
               origin_gating::GateableEvent::kNavigationRequest,
               // Gate navigations to prevent the actor from navigating to
               // unapproved or dangerous destinations.
               origin_gating::GateableEvent::kNavigationResponse,
               // Gate user/actor page interactions (clicks, from inputs,
               // etc.) within the loaded page.
               origin_gating::GateableEvent::kPageAction}),
      },
      // Do not cache decisions per-site, as actor safety policies require
      // re-evaluating each navigation and page action dynamically.
      /*use_site_keyed_cache=*/false);
}

void ActorEngine::ExecuteNextAction() {
  if (next_action_index_ >= action_sequence_.size()) {
    CompleteActions(ActionResult(ToolExecutionResult::Ok()));
    return;
  }

  next_action_index_++;

  // TODO(crbug.com/496196533): Add pre-execution checks.
  SetState(State::kPreExecutionChecks);

  // TODO(crbug.com/503072595): Add tool verification.
  SetState(State::kToolVerify);

  // TODO(crbug.com/496195979): Add UI pre-invoke.
  UiPreInvoke();
}

void ActorEngine::UiPreInvoke() {
  SetState(State::kUiPreInvoke);

  const ActorToolRequest* action =
      action_sequence_[InProgressActionIndex()].get();
  if (!action) {
    FinishedUiPreInvoke(ActionResult(
        ToolExecutionResult(mojom::ActionResultCode::kToolUnknown)));
    return;
  }

  execution_updates_delegate_->OnWillExecuteTool(
      action->GetToolType(), GetWebStateIDForAction(action));

  FinishedUiPreInvoke(ActionResult(ToolExecutionResult::Ok()));
}

void ActorEngine::SetState(State new_state) {
  // TODO(crbug.com/503841160): Log the proper WebState URLs.
  LogJournalEvent(GetJournal(), GURL(), GetTaskId(),
                  "ExecutionEngine::StateChange",
                  {{"current_state", ActorEngineStateToString(state_)},
                   {"new_state", ActorEngineStateToString(new_state)}});
  state_ = new_state;
}

void ActorEngine::FinishedUiPreInvoke(ActionResult result) {
  if (!result.tool_result.IsOk()) {
    CompleteActions(std::move(result));
    return;
  }

  SetState(State::kToolInvoke);

  const ActorToolRequest* action =
      action_sequence_[InProgressActionIndex()].get();
  tool_controller_ = std::make_unique<ToolController>(/*tool_delegate=*/this);
  tool_controller_->CreateToolAndValidate(
      *action, base::BindOnce(&ActorEngine::OnToolValidationComplete,
                              weak_ptr_factory_.GetWeakPtr()));
}

void ActorEngine::OnToolValidationComplete(ToolExecutionResult result) {
  if (!result.IsOk()) {
    OnToolExecutionComplete(std::move(result));
    return;
  }
  LogToolExecutionResult(
      GetJournal(), GURL(), GetTaskId(),
      /*event_name=*/
      base::StringPrintf("CreateTool #%zu", InProgressActionIndex()), result,
      /*success_details_key=*/"ActorEngine::FinishedUiPreInvoke");

  tool_controller_->Invoke(base::BindOnce(&ActorEngine::OnToolExecutionComplete,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void ActorEngine::OnToolExecutionComplete(ToolExecutionResult result) {
  FinishedToolInvoke(ActionResult(result));
}

void ActorEngine::FinishedToolInvoke(ActionResult result) {
  bool success = result.tool_result.IsOk();

  if (!success) {
    CompleteActions(std::move(result));
    return;
  }

  action_results_.push_back(std::move(result));

  // TODO(crbug.com/496195979): Add UI post-invoke.
  SetState(State::kUiPostInvoke);
  FinishedUiPostInvoke(ActionResult(ToolExecutionResult::Ok()));
}

void ActorEngine::FinishedUiPostInvoke(ActionResult result) {
  if (!result.tool_result.IsOk()) {
    CompleteActions(std::move(result));
    return;
  }
  ExecuteNextAction();
}

void ActorEngine::CompleteActions(ActionResult result) {
  bool success = result.tool_result.IsOk();

  // Successful tool results are already appended in `FinishedToolInvoke`,
  // therefore only record/overwrite the result if it is a failure.
  if (!success) {
    size_t index = InProgressActionIndex();
    if (action_results_.size() == index) {
      // This is the first result for the current action, append to results
      // vector.
      action_results_.push_back(std::move(result));
    } else if (action_results_.size() > index) {
      // A result was already recorded for this action. Overwrite the success
      // result with the failure.
      action_results_[index] = std::move(result);
    }
  }

  SetState(success ? State::kCompleted : State::kFailed);

  action_sequence_.clear();

  if (completion_callback_) {
    std::move(completion_callback_).Run(std::move(action_results_));
  }
}

size_t ActorEngine::InProgressActionIndex() const {
  CHECK_GT(next_action_index_, 0ul);
  return next_action_index_ - 1;
}

}  // namespace actor
