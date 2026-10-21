// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"

#import <algorithm>
#import <optional>
#import <set>

#import "base/barrier_callback.h"
#import "base/base64.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/journal_details_builder.h"
#import "components/actor/core/task_source_info.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_enterprise_policy_checker_ios.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task_background_worker.h"
#import "ios/chrome/browser/intelligence/actor/model/posted_observer_list.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_lifecycle_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_browser_utils.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/logging_util.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace actor {

namespace {

// Returns the code of the first failed action in `results`, or `kOk` if every
// action succeeded.
mojom::ActionResultCode GetFirstFailedActionResultCode(
    const std::vector<ActionResult>& results) {
  for (const ActionResult& result : results) {
    if (!result.tool_result.IsOk()) {
      return result.tool_result.code();
    }
  }
  return mojom::ActionResultCode::kOk;
}

// Serializes and logs the given batch of `actions` as an Actions proto
// associated with `task_id` to `journal` for Protoshop inspection and Perfetto
// tracing.
void LogActionsProto(
    AggregatedJournal* journal,
    ActorTaskId task_id,
    const std::vector<optimization_guide::proto::Action>& actions) {
  if (!journal) {
    return;
  }
  optimization_guide::proto::Actions actions_proto;
  actions_proto.set_task_id(task_id.value());
  actions_proto.mutable_actions()->Reserve(actions.size());
  for (const auto& action : actions) {
    *actions_proto.add_actions() = action;
  }
  journal->LogProto(GURL(), task_id, "PerformActions", /*details=*/{},
                    actions_proto,
                    "chrome_intelligence_proto_features.Actions");
}

// Posts `callback` with `result`. Early replies are posted so that callers are
// never re-entered from within `ActorService::PerformActions()`.
void PostPerformActionsReply(PerformActionsCallback callback,
                             PerformActionsResult result) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

}  // namespace

ActorService::ActorService(ProfileIOS* profile)
    : profile_(profile),
      tool_factory_(std::make_unique<ActorToolFactory>(profile)),
      journal_(std::make_unique<AggregatedJournal>()),
      policy_checker_(std::make_unique<ActorEnterprisePolicyCheckerIOS>(
          profile ? profile->GetPrefs() : nullptr)),
      lifecycle_observers_([[PostedObserverList alloc]
          initWithProtocol:@protocol(ActorTaskLifecycleObserver)]) {
  CHECK(tool_factory_);
  can_act_on_web_subscription_ =
      policy_checker_->AddCanActOnWebChangedCallback(base::BindRepeating(
          &ActorService::OnCanActOnWebChanged, weak_ptr_factory_.GetWeakPtr()));
}

ActorService::~ActorService() {
  Shutdown();
}

void ActorService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  can_act_on_web_subscription_ = {};
  // Pending lifecycle notifications are dropped, so that observers are never
  // called back after `Shutdown()`.
  [lifecycle_observers_ removeAllObservers];
  // TODO(crbug.com/568342921): Gracefully cleanup tasks.
  task_observers_.clear();
  active_tasks_.clear();
  policy_checker_.reset();
}

ActorTaskId ActorService::CreateTask(const std::string& title,
                                     const TaskSourceInfo& source_info,
                                     bool allow_incognito_web_states) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsActorEnabled());
  if (policy_checker_ && !policy_checker_->CanActOnWeb()) {
    journal_->Log(GURL(), ActorTaskId(), "ActorService::CreateTask",
                  JournalDetailsBuilder()
                      .AddError("Blocked by enterprise policy")
                      .Build());
    // TODO(crbug.com/567036075): Signal task creation failure to callers and
    // handle tasks not created properly at the caller level.
    return ActorTaskId();
  }

  const ActorTaskId task_id = next_task_id_.GenerateNextId();
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
  auto task = std::make_unique<ActorTask>(
      task_id, title, source_info, allow_incognito_web_states, journal_.get(),
      tool_factory_.get(), browser_list, policy_checker_.get());

  for (id<ActorTaskUpdatesObserver> observer : task_observers_) {
    if (observer) {
      task->AddObserver(observer);
    }
  }

  active_tasks_[task_id] = std::move(task);
  // Copied because blocks capture C++ references by reference, and
  // `source_info` may not outlive the posted notification.
  const TaskSourceInfo source = source_info;
  [lifecycle_observers_
      postNotification:^(id<ActorTaskLifecycleObserver> observers) {
        [observers actorServiceDidStartTaskWithID:task_id sourceInfo:source];
      }];
  return task_id;
}

void ActorService::PerformActions(
    ActorTaskId task_id,
    const std::vector<optimization_guide::proto::Action>& actions,
    const std::string& task_update,
    PerformActionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsActorEnabled());

  LogActionsProto(journal_.get(), task_id, actions);

  auto it = active_tasks_.find(task_id);
  if (it == active_tasks_.end()) {
    PerformActionsResult actions_result;
    actions_result.action_results =
        MakeActionResults(mojom::ActionResultCode::kTaskWentAway);
    PostPerformActionsReply(std::move(callback), std::move(actions_result));
    return;
  }

  if (policy_checker_ && !policy_checker_->CanActOnWeb()) {
    constexpr mojom::ActionResultCode kRejectionCode =
        mojom::ActionResultCode::kActionsBlockedByEnterprisePolicy;
    LogActRejection(*journal_, task_id, "ActorService::PerformActions",
                    kRejectionCode);
    PerformActionsResult actions_result;
    actions_result.action_results = MakeActionResults(kRejectionCode);
    PostPerformActionsReply(std::move(callback), std::move(actions_result));
    return;
  }

  // Reject before `AddControlledWebStates`, so that a rejected request does not
  // leave its target WebStates controlled by the task.
  // TODO(crbug.com/568338837): Rely on the `ActorTask::Act()` check once the
  // task registers its controlled WebStates.
  if (it->second->HasPendingAct()) {
    constexpr mojom::ActionResultCode kRejectionCode =
        mojom::ActionResultCode::kExecutionEngineExistingAction;
    LogActRejection(*journal_, task_id, "ActorService::PerformActions",
                    kRejectionCode);
    PerformActionsResult actions_result;
    actions_result.action_results = MakeActionResults(kRejectionCode);
    PostPerformActionsReply(std::move(callback), std::move(actions_result));
    return;
  }

  std::vector<std::unique_ptr<ActorToolRequest>> tool_requests;
  tool_requests.reserve(actions.size());
  for (const auto& action : actions) {
    tool_requests.push_back(std::make_unique<ActorToolRequest>(action));
  }

  // Resolve and register target WebStates from requests before execution.
  AddControlledWebStates(it->second.get(), tool_requests);

  it->second->Act(std::move(tool_requests), task_update,
                  base::BindOnce(&ActorService::OnActCompleted,
                                 weak_ptr_factory_.GetWeakPtr(), task_id,
                                 std::move(callback)));
}

void ActorService::RequestTabObservation(ActorTaskId task_id,
                                         web::WebState* web_state,
                                         TabObservationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = active_tasks_.find(task_id);
  if (it == active_tasks_.end() || !web_state) {
    std::move(callback).Run(PageContextWrapperCallbackResponse());
    return;
  }

  const GURL last_committed_url = web_state->GetLastCommittedURL();
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry =
      journal_->CreatePendingAsyncEntry(
          last_committed_url, task_id, MakeBrowserTrackUUID(task_id),
          "RequestTabObservation", /*details=*/{});

  PageContextWrapperConfigBuilder builder;
  builder.SetUseRichExtraction(true);
  builder.SetUseRichExtractionWithActionable(true);
  builder.SetIncludeSameSiteOnly(true);
  builder.SetSnapshotCachePolicy(
      PageContextWrapperConfig::SnapshotCachePolicy::kBypassCache);
  PageContextWrapperConfig config = builder.Build();

  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

  PageContextWrapper* page_context_wrapper = [[PageContextWrapper alloc]
        initWithWebState:web_state
                  config:config
      completionCallback:base::BindOnce(
                             &ActorService::OnPageContextExtractionComplete,
                             weak_ptr_factory_.GetWeakPtr(), web_state_id,
                             last_committed_url, std::move(journal_entry),
                             std::move(callback))];

  pending_observations_[web_state_id] = page_context_wrapper;

  [page_context_wrapper setShouldGetAnnotatedPageContent:YES];
  [page_context_wrapper setShouldGetSnapshot:YES];
  [page_context_wrapper populatePageContextFieldsAsyncWithTimeout:
                            GetActorTabObservationPageContextTimeout()];
}

void ActorService::PauseTask(ActorTaskId task_id, bool from_actor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/496163986): Implement and test.
}

void ActorService::SetTaskInterventionDelegate(
    ActorTaskId task_id,
    id<ActorTaskInterventionDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = active_tasks_.find(task_id);
  if (it != active_tasks_.end()) {
    it->second->SetInterventionDelegate(delegate);
  }
}

void ActorService::InterruptTask(ActorTaskId task_id,
                                 ActorTaskInterruptReason reason,
                                 std::string_view message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = active_tasks_.find(task_id);
  if (it == active_tasks_.end()) {
    return;
  }
  it->second->Interrupt(reason, message);
  // TODO(crbug.com/532978481): Clean up once `ActorTask` routes
  // task-initiated stops directly through `ActorService::StopTask`.
  it = active_tasks_.find(task_id);
  if (it != active_tasks_.end() && IsTerminalState(it->second->GetState())) {
    NotifyTaskStopped(*it->second);
    active_tasks_.erase(it);
  }
}

void ActorService::StopTask(ActorTaskId task_id,
                            ActorTaskStoppedReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/496163986): Implement and test.
  auto it = active_tasks_.find(task_id);
  if (it != active_tasks_.end()) {
    it->second->Stop(reason);
    NotifyTaskStopped(*it->second);
  }
  active_tasks_.erase(task_id);
}

// TODO(crbug.com/517583120): Remove when the temporary actuation prototype is
// cleaned up.
void ActorService::StopAllTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!active_tasks_.empty()) {
    StopTask(active_tasks_.begin()->first,
             ActorTaskStoppedReason::kStoppedByUser);
  }
}

// TODO(crbug.com/556295233): Remove once the granular observer is implemented.
void ActorService::AddTaskUpdatesObserver(
    id<ActorTaskUpdatesObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(observer);
  if (std::ranges::contains(task_observers_, observer)) {
    return;
  }
  std::erase_if(task_observers_, [](id obs) { return obs == nil; });
  task_observers_.push_back(observer);
  for (const auto& [task_id, task] : active_tasks_) {
    task->AddObserver(observer);
  }
}

// TODO(crbug.com/556295233): Remove once the granular observer is implemented.
void ActorService::RemoveTaskUpdatesObserver(
    id<ActorTaskUpdatesObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::erase(task_observers_, observer);
  for (const auto& [task_id, task] : active_tasks_) {
    task->RemoveObserver(observer);
  }
}

bool ActorService::AddTaskUpdatesObserver(
    ActorTaskId task_id,
    id<ActorTaskUpdatesObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(observer);
  auto it = active_tasks_.find(task_id);
  if (it == active_tasks_.end()) {
    return false;
  }
  it->second->AddObserver(observer);
  return true;
}

void ActorService::RemoveTaskUpdatesObserver(
    ActorTaskId task_id,
    id<ActorTaskUpdatesObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = active_tasks_.find(task_id);
  if (it == active_tasks_.end()) {
    return;
  }
  it->second->RemoveObserver(observer);
}

void ActorService::AddTaskLifecycleObserver(
    id<ActorTaskLifecycleObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(observer);
  [lifecycle_observers_ addObserver:observer];
}

void ActorService::RemoveTaskLifecycleObserver(
    id<ActorTaskLifecycleObserver> observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [lifecycle_observers_ removeObserver:observer];
}

std::optional<ActorTaskState> ActorService::GetActiveTaskState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (active_tasks_.empty()) {
    return std::nullopt;
  }
  return active_tasks_.rbegin()->second->GetState();
}

web::WebState* ActorService::GetWebStateForID(web::WebStateID web_state_id,
                                              ActorTaskId task_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = active_tasks_.find(task_id);
  if (it == active_tasks_.end()) {
    return nullptr;
  }

  for (const auto& weak_ptr : it->second->controlled_web_states()) {
    if (weak_ptr && weak_ptr->GetUniqueIdentifier() == web_state_id) {
      return weak_ptr.get();
    }
  }

  return nullptr;
}

void ActorService::AddControlledWebState(ActorTaskId task_id,
                                         web::WebState* web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsActorEnabled());

  auto it = active_tasks_.find(task_id);
  if (it != active_tasks_.end()) {
    it->second->AddControlledWebState(web_state);
  }
}

std::vector<base::WeakPtr<ActorTaskBackgroundWorker>>
ActorService::GetWeakTaskBackgroundWorkers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<base::WeakPtr<ActorTaskBackgroundWorker>> workers;
  workers.reserve(active_tasks_.size());
  for (const auto& [task_id, task] : active_tasks_) {
    workers.push_back(task->background_worker()->GetWeakPtr());
  }
  return workers;
}

#pragma mark - Private

void ActorService::NotifyTaskStopped(const ActorTask& task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const ActorTaskId task_id = task.task_id();
  const TaskSourceInfo source = task.source_info();
  const ActorTaskState final_state = task.GetState();
  [lifecycle_observers_
      postNotification:^(id<ActorTaskLifecycleObserver> observers) {
        [observers actorServiceDidStopTaskWithID:task_id
                                      sourceInfo:source
                                      finalState:final_state];
      }];
}

void ActorService::OnPageContextExtractionComplete(
    web::WebStateID web_state_id,
    const GURL& last_committed_url,
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> pending_journal_entry,
    TabObservationCallback callback,
    PageContextWrapperCallbackResponse response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_observations_.erase(web_state_id);

  if (pending_journal_entry) {
    if (!response.has_value() || !response.value()) {
      pending_journal_entry->EndEntry(
          JournalDetailsBuilder()
              .AddError("Failed to extract page context")
              .Build());
    } else {
      const optimization_guide::proto::PageContext& page_context =
          *response.value();
      if (page_context.has_annotated_page_content()) {
        const auto& apc = page_context.annotated_page_content();
        std::vector<uint8_t> buffer(apc.ByteSizeLong());
        if (apc.SerializeToArray(buffer.data(), buffer.size())) {
          pending_journal_entry->GetJournal().LogAnnotatedPageContent(
              last_committed_url, pending_journal_entry->GetTaskId(), buffer);
        }
      }
      if (page_context.has_tab_screenshot()) {
        std::optional<std::vector<uint8_t>> decoded_screenshot =
            base::Base64Decode(page_context.tab_screenshot());
        if (decoded_screenshot.has_value()) {
          pending_journal_entry->GetJournal().LogScreenshot(
              last_committed_url, pending_journal_entry->GetTaskId(),
              "image/png", *decoded_screenshot,
              /*iframe_data=*/std::nullopt);
        }
      }
      pending_journal_entry->EndEntry({});
    }
  }

  std::move(callback).Run(std::move(response));
}

void ActorService::OnActCompleted(ActorTaskId task_id,
                                  PerformActionsCallback callback,
                                  std::vector<ActionResult> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const mojom::ActionResultCode result_code =
      GetFirstFailedActionResultCode(results);
  // Matches the entry logged by Desktop's `ActorKeyedServiceProtoWrapper`.
  journal_->Log(
      GURL(), task_id, "PerformActionsFinished",
      JournalDetailsBuilder().Add("result_code", result_code).Build());

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

  // Spans every tab observation of this batch so that each
  // `RequestTabObservation` slice nests under it on the Browser track, as on
  // Desktop. iOS always captures the screenshot and the APC, so the observation
  // never skips them.
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> observation_entry =
      journal_->CreatePendingAsyncEntry(
          GURL(), task_id, MakeBrowserTrackUUID(task_id),
          "TabObservationController",
          JournalDetailsBuilder()
              .Add("result_code", result_code)
              .Add("skip_async_observation_information", false)
              .Build());

  // Barrier to wait for all PageContext extractions and add them to
  // ActionsResult.
  auto barrier = base::BarrierCallback<std::unique_ptr<TabObservationResponse>>(
      web_states_to_extract.size(),
      base::BindOnce(
          [](std::unique_ptr<AggregatedJournal::PendingAsyncEntry>
                 observation_entry,
             PerformActionsCallback callback, PerformActionsResult result,
             std::vector<std::unique_ptr<TabObservationResponse>> contexts) {
            // Ended before replying so that the slice only covers the
            // observation, and after every `RequestTabObservation` slice so
            // that those stay nested under it.
            observation_entry->EndEntry(/*details=*/{});
            result.page_contexts = std::move(contexts);
            std::move(callback).Run(std::move(result));
          },
          std::move(observation_entry), std::move(callback),
          std::move(perform_actions_result)));

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

void ActorService::AddControlledWebStates(
    ActorTask* task,
    const std::vector<std::unique_ptr<ActorToolRequest>>& actions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsActorEnabled());
  CHECK(task);

  for (const auto& request : actions) {
    // The vector is populated by `CreateActorToolRequests` with non-null
    // entries, and since `AddControlledWebStates` is called before the vector
    // is moved to the task, the pointers are guaranteed to still be valid.
    CHECK(request);
    web::WebStateID target_id = request->GetTargetWebStateId();
    if (!target_id.valid()) {
      continue;
    }

    web::WebState* web_state =
        GetWebState(target_id, task->allow_incognito_web_states());
    if (web_state) {
      task->AddControlledWebState(web_state);
    }
  }
}

web::WebState* ActorService::GetWebState(web::WebStateID web_state_id,
                                         bool allows_incognito) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BrowserAndIndex browser_and_index =
      FindBrowserAndIndexFromProfile(profile_, web_state_id, allows_incognito);

  if (browser_and_index.tab_index == WebStateList::kInvalidIndex ||
      !browser_and_index.browser) {
    return nullptr;
  }

  return browser_and_index.browser->GetWebStateList()->GetWebStateAt(
      browser_and_index.tab_index);
}

void ActorService::OnCanActOnWebChanged(bool can_act_on_web) {
  if (can_act_on_web) {
    return;
  }

  while (!active_tasks_.empty()) {
    StopTask(active_tasks_.begin()->first,
             ActorTaskStoppedReason::kBrowserFailure);
  }
}
}  // namespace actor
