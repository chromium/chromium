// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_manager_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "content/browser/tracing/background_startup_tracing_observer.h"
#include "content/browser/tracing/background_tracing_active_scenario.h"
#include "content/browser/tracing/background_tracing_agent_client_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/common/child_process.mojom.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

const char kBackgroundTracingConfig[] = "config";

}  // namespace

// static
const char BackgroundTracingManager::kContentTriggerConfig[] =
    "content-trigger-config";

// static
BackgroundTracingManager& BackgroundTracingManager::GetInstance() {
  return BackgroundTracingManagerImpl::GetInstance();
}

// static
void BackgroundTracingManagerImpl::RecordMetric(Metrics metric) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.ScenarioState", metric,
                            Metrics::NUMBER_OF_BACKGROUND_TRACING_METRICS);
}

// static
BackgroundTracingManagerImpl& BackgroundTracingManagerImpl::GetInstance() {
  static base::NoDestructor<BackgroundTracingManagerImpl> manager;
  return *manager;
}

// static
void BackgroundTracingManagerImpl::ActivateForProcess(
    int child_process_id,
    mojom::ChildProcess* child_process) {
  // NOTE: Called from any thread.

  mojo::PendingRemote<tracing::mojom::BackgroundTracingAgentProvider>
      pending_provider;
  child_process->GetBackgroundTracingAgentProvider(
      pending_provider.InitWithNewPipeAndPassReceiver());

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BackgroundTracingManagerImpl::AddPendingAgent,
                                child_process_id, std::move(pending_provider)));
}

BackgroundTracingManagerImpl::BackgroundTracingManagerImpl()
    : delegate_(GetContentClient()->browser()->GetTracingDelegate()) {
  BackgroundStartupTracingObserver::GetInstance();
}

BackgroundTracingManagerImpl::~BackgroundTracingManagerImpl() = default;

void BackgroundTracingManagerImpl::AddMetadataGeneratorFunction() {
  auto* metadata_source = tracing::TraceEventMetadataSource::GetInstance();
  metadata_source->AddGeneratorFunction(
      base::BindRepeating(&BackgroundTracingManagerImpl::GenerateMetadataProto,
                          base::Unretained(this)));
}

bool BackgroundTracingManagerImpl::SetActiveScenario(
    std::unique_ptr<BackgroundTracingConfig> config,
    DataFiltering data_filtering) {
  // Pass a null ReceiveCallback to use the default upload behaviour.
  return SetActiveScenarioWithReceiveCallback(
      std::move(config), ReceiveCallback(), data_filtering);
}

bool BackgroundTracingManagerImpl::SetActiveScenarioWithReceiveCallback(
    std::unique_ptr<BackgroundTracingConfig> config,
    ReceiveCallback receive_callback,
    DataFiltering data_filtering) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (config) {
    RecordMetric(Metrics::SCENARIO_ACTIVATION_REQUESTED);
  }

  if (active_scenario_ && (active_scenario_->state() !=
                           BackgroundTracingActiveScenario::State::kIdle)) {
    return false;
  }

  // If we don't have a high resolution timer available, traces will be
  // too inaccurate to be useful.
  if (!base::TimeTicks::IsHighResolution()) {
    if (config) {
      RecordMetric(Metrics::SCENARIO_ACTION_FAILED_LOWRES_CLOCK);
    }
    return false;
  }

  std::unique_ptr<BackgroundTracingConfigImpl> config_impl(
      static_cast<BackgroundTracingConfigImpl*>(config.release()));
  config_impl = BackgroundStartupTracingObserver::GetInstance()
                    .IncludeStartupConfigIfNeeded(std::move(config_impl));
  if (BackgroundStartupTracingObserver::GetInstance()
          .enabled_in_current_session()) {
    // Anonymize data for startup tracing by default. We currently do not
    // support storing the config in preferences for next session.
    data_filtering = DataFiltering::ANONYMIZE_DATA;
    RecordMetric(Metrics::STARTUP_SCENARIO_TRIGGERED);
  } else {
    // If startup config was not set and we're not a SYSTEM scenario (system
    // might already have started a trace in the background) but tracing was
    // enabled, then do not set any scenario.
    if (base::trace_event::TraceLog::GetInstance()->IsEnabled() &&
        config_impl &&
        config_impl->tracing_mode() != BackgroundTracingConfigImpl::SYSTEM) {
      return false;
    }
  }

  if (!config_impl) {
    return false;
  }

  bool requires_anonymized_data = (data_filtering == ANONYMIZE_DATA);
  config_impl->set_requires_anonymized_data(requires_anonymized_data);

  // TODO(oysteine): Retry when time_until_allowed has elapsed.
  if (config_impl && delegate_ &&
      !delegate_->IsAllowedToBeginBackgroundScenario(
          config_impl->scenario_name(), requires_anonymized_data,
          config_impl->has_crash_scenario())) {
    return false;
  }

  active_scenario_ = std::make_unique<BackgroundTracingActiveScenario>(
      std::move(config_impl), std::move(receive_callback),
      base::BindOnce(&BackgroundTracingManagerImpl::OnScenarioAborted,
                     base::Unretained(this)));

  if (BackgroundStartupTracingObserver::GetInstance()
          .enabled_in_current_session()) {
    EmitNamedTrigger(kStartupTracingTriggerName);
  }

  active_scenario_->StartTracingIfConfigNeedsIt();
  RecordMetric(Metrics::SCENARIO_ACTIVATED_SUCCESSFULLY);

  return true;
}

bool BackgroundTracingManagerImpl::HasActiveScenario() {
  return !!active_scenario_;
}

bool BackgroundTracingManagerImpl::HasTraceToUpload() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Send the logs only when the trace size is within limits. If the connection
  // type changes and we have a bigger than expected trace, then the next time
  // service asks us when wifi is available, the trace will be sent. If we did
  // collect a trace that is bigger than expected, then we will end up never
  // uploading, and drop the trace. This should never happen because the trace
  // buffer limits are set appropriately.
  if (trace_to_upload_.empty()) {
    return false;
  }
  if (active_scenario_ &&
      trace_to_upload_.size() <=
          active_scenario_->GetTraceUploadLimitKb() * 1024) {
    return true;
  }
  RecordMetric(Metrics::LARGE_UPLOAD_WAITING_TO_RETRY);
  return false;
}

std::string BackgroundTracingManagerImpl::GetLatestTraceToUpload() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string ret;
  ret.swap(trace_to_upload_);

  if (active_scenario_) {
    active_scenario_->OnFinalizeComplete(true);
  }

  return ret;
}

void BackgroundTracingManagerImpl::AddEnabledStateObserverForTesting(
    BackgroundTracingManager::EnabledStateTestObserver* observer) {
  // Ensure that this code is called on the UI thread, except for
  // tests where a UI thread might not have been initialized at this point.
  DCHECK(
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) ||
      !content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));
  background_tracing_observers_.insert(observer);
}

void BackgroundTracingManagerImpl::RemoveEnabledStateObserverForTesting(
    BackgroundTracingManager::EnabledStateTestObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  background_tracing_observers_.erase(observer);
}

void BackgroundTracingManagerImpl::AddAgent(
    tracing::mojom::BackgroundTracingAgent* agent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agents_.insert(agent);

  for (auto* observer : agent_observers_) {
    observer->OnAgentAdded(agent);
  }
}

void BackgroundTracingManagerImpl::RemoveAgent(
    tracing::mojom::BackgroundTracingAgent* agent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto* observer : agent_observers_) {
    observer->OnAgentRemoved(agent);
  }

  agents_.erase(agent);
}

void BackgroundTracingManagerImpl::AddAgentObserver(AgentObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agent_observers_.insert(observer);

  MaybeConstructPendingAgents();

  for (auto* agent : agents_) {
    observer->OnAgentAdded(agent);
  }
}

void BackgroundTracingManagerImpl::RemoveAgentObserver(
    AgentObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agent_observers_.erase(observer);

  for (auto* agent : agents_) {
    observer->OnAgentRemoved(agent);
  }
}

BackgroundTracingActiveScenario*
BackgroundTracingManagerImpl::GetActiveScenarioForTesting() {
  DCHECK(active_scenario_);
  return active_scenario_.get();
}

bool BackgroundTracingManagerImpl::IsTracingForTesting() {
  return active_scenario_ && (active_scenario_->state() ==
                              BackgroundTracingActiveScenario::State::kTracing);
}

void BackgroundTracingManagerImpl::SetTraceToUploadForTesting(
    std::unique_ptr<std::string> trace_data) {
  SetTraceToUpload(std::move(trace_data));
}

void BackgroundTracingManagerImpl::SetConfigTextFilterForTesting(
    ConfigTextFilterForTesting predicate) {
  config_text_filter_for_testing_ = std::move(predicate);
}

void BackgroundTracingManagerImpl::SetTraceToUpload(
    std::unique_ptr<std::string> trace_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (trace_data) {
    trace_to_upload_.swap(*trace_data);
  } else {
    trace_to_upload_.clear();
  }
}

std::unique_ptr<content::BackgroundTracingConfig>
BackgroundTracingManagerImpl::GetBackgroundTracingConfig(
    const std::string& trial_name) {
  std::string config_text =
      base::GetFieldTrialParamValue(trial_name, kBackgroundTracingConfig);
  if (config_text.empty())
    return nullptr;

  if (config_text_filter_for_testing_)
    config_text = config_text_filter_for_testing_.Run(config_text);

  auto value = base::JSONReader::Read(config_text);
  if (!value)
    return nullptr;

  if (!value->is_dict())
    return nullptr;

  return BackgroundTracingConfig::FromDict(std::move(*value).TakeDict());
}

void BackgroundTracingManagerImpl::SetNamedTriggerCallback(
    const std::string& trigger_name,
    base::RepeatingCallback<bool()> callback) {
  if (!callback) {
    named_trigger_callbacks_.erase(trigger_name);
  } else {
    named_trigger_callbacks_.emplace(trigger_name, std::move(callback));
  }
}

bool BackgroundTracingManagerImpl::EmitNamedTrigger(
    const std::string& trigger_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = named_trigger_callbacks_.find(trigger_name);
  if (it == named_trigger_callbacks_.end()) {
    return false;
  }
  return it->second.Run();
}

void BackgroundTracingManagerImpl::InvalidateTriggersCallbackForTesting() {
  named_trigger_callbacks_.clear();
}

void BackgroundTracingManagerImpl::OnStartTracingDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto* observer : background_tracing_observers_) {
    observer->OnTracingEnabled();
  }
}

bool BackgroundTracingManagerImpl::IsAllowedFinalization(
    bool is_crash_scenario) const {
  return !delegate_ ||
         (active_scenario_ &&
          delegate_->IsAllowedToEndBackgroundScenario(
              active_scenario_->GetConfig()->scenario_name(),
              active_scenario_->GetConfig()->requires_anonymized_data(),
              is_crash_scenario));
}

void BackgroundTracingManagerImpl::GenerateMetadataProto(
    perfetto::protos::pbzero::ChromeMetadataPacket* metadata,
    bool privacy_filtering_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (active_scenario_) {
    active_scenario_->GenerateMetadataProto(metadata);
  }
}

void BackgroundTracingManagerImpl::AbortScenarioForTesting() {
  AbortScenario();
}

void BackgroundTracingManagerImpl::AbortScenario() {
  if (active_scenario_) {
    active_scenario_->AbortScenario();
  }
}

void BackgroundTracingManagerImpl::OnScenarioAborted() {
  DCHECK(active_scenario_);

  // Don't synchronously delete to avoid use-after-free issues in
  // BackgroundTracingActiveScenario.
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(active_scenario_));

  for (auto* observer : background_tracing_observers_) {
    observer->OnScenarioAborted();
  }
}

// static
void BackgroundTracingManagerImpl::AddPendingAgent(
    int child_process_id,
    mojo::PendingRemote<tracing::mojom::BackgroundTracingAgentProvider>
        pending_provider) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Delay agent initialization until we have an interested AgentObserver.
  // We set disconnect handler for cleanup when the tracing target is closed.
  mojo::Remote<tracing::mojom::BackgroundTracingAgentProvider> provider(
      std::move(pending_provider));

  provider.set_disconnect_handler(base::BindOnce(
      &BackgroundTracingManagerImpl::ClearPendingAgent, child_process_id));

  GetInstance().pending_agents_[child_process_id] = std::move(provider);
  GetInstance().MaybeConstructPendingAgents();
}

// static
void BackgroundTracingManagerImpl::ClearPendingAgent(int child_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetInstance().pending_agents_.erase(child_process_id);
}

void BackgroundTracingManagerImpl::MaybeConstructPendingAgents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (agent_observers_.empty())
    return;

  for (auto& pending_agent : pending_agents_) {
    pending_agent.second.set_disconnect_handler(base::OnceClosure());
    BackgroundTracingAgentClientImpl::Create(pending_agent.first,
                                             std::move(pending_agent.second));
  }
  pending_agents_.clear();
}

}  // namespace content
