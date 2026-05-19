// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_manager_impl.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "content/browser/tracing/background_tracing_agent_client_impl.h"
#include "content/common/child_process.mojom.h"
#include "content/public/browser/background_tracing.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"
#include "services/tracing/public/cpp/trace_startup_config.h"

namespace content {
namespace {

BackgroundTracingManagerImpl* g_background_tracing_manager_impl = nullptr;

class PreferenceManagerImpl
    : public BackgroundTracingManagerImpl::PreferenceManager {
  bool GetBackgroundStartupTracingEnabled() const override {
    return tracing::TraceStartupConfig::GetInstance().IsEnabled() &&
           tracing::TraceStartupConfig::GetInstance().GetSessionOwner() ==
               tracing::TraceStartupConfig::SessionOwner::kBackgroundTracing;
  }
};

}  // namespace

// static
BackgroundTracingManagerImpl& BackgroundTracingManagerImpl::GetInstance() {
  CHECK_NE(nullptr, g_background_tracing_manager_impl);
  return *g_background_tracing_manager_impl;
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

BackgroundTracingManagerImpl::BackgroundTracingManagerImpl(
    TracingDelegate* delegate)
    : delegate_(delegate), state_manager_(delegate_->CreateStateManager()) {
  g_background_tracing_manager_impl = this;
  TracingAgentObserverManager::SetInstance(this);
  preferences_ = std::make_unique<PreferenceManagerImpl>();
}

BackgroundTracingManagerImpl::~BackgroundTracingManagerImpl() {
  DCHECK_EQ(this, g_background_tracing_manager_impl);
  g_background_tracing_manager_impl = nullptr;
  DisableScenarios();
  TracingAgentObserverManager::SetInstance(nullptr);
}

bool BackgroundTracingManagerImpl::IsRecordingAllowed(
    bool privacy_filter_enabled,
    base::TimeTicks scenario_start_time) {
  return delegate_->IsRecordingAllowed(privacy_filter_enabled,
                                       scenario_start_time);
}

bool BackgroundTracingManagerImpl::ShouldSaveUnuploadedTrace() {
  return delegate_->ShouldSaveUnuploadedTrace();
}

std::string
BackgroundTracingManagerImpl::RecordSerializedSystemProfileMetrics() {
  return delegate_->RecordSerializedSystemProfileMetrics();
}

std::optional<base::FilePath>
BackgroundTracingManagerImpl::GetLocalTracesDirectory() {
  return GetContentClient()->browser()->GetLocalTracesDirectory();
}

bool BackgroundTracingManagerImpl::GetBackgroundStartupTracingEnabled() const {
  return preferences_->GetBackgroundStartupTracingEnabled();
}

void BackgroundTracingManagerImpl::SetPreferenceManagerForTesting(
    std::unique_ptr<PreferenceManager> preferences) {
  preferences_ = std::move(preferences);
}

std::vector<traces_internals::mojom::ScenarioPtr>
BackgroundTracingManagerImpl::GetAllScenarios() const {
  std::vector<traces_internals::mojom::ScenarioPtr> result;
  auto toMojoScenario = [this](tracing::TracingScenario* scenario) {
    auto new_scenario = traces_internals::mojom::Scenario::New();
    new_scenario->scenario_name = scenario->scenario_name();
    new_scenario->description = scenario->description();
    new_scenario->is_local_scenario = scenario->is_local_scenario();
    new_scenario->is_enabled =
        std::ranges::contains(enabled_scenarios_, scenario);
    new_scenario->current_state = scenario->current_state();
    return new_scenario;
  };
  for (const auto& scenario : preset_scenarios_) {
    result.push_back(toMojoScenario(scenario.second.get()));
  }
  for (const auto& scenario : field_scenarios_) {
    result.push_back(toMojoScenario(scenario.get()));
  }
  return result;
}

void BackgroundTracingManagerImpl::AddAgent(
    tracing::mojom::BackgroundTracingAgent* agent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agents_.insert(agent);

  for (AgentObserver* observer : agent_observers_) {
    observer->OnAgentAdded(agent);
  }
}

void BackgroundTracingManagerImpl::RemoveAgent(
    tracing::mojom::BackgroundTracingAgent* agent) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (AgentObserver* observer : agent_observers_) {
    observer->OnAgentRemoved(agent);
  }

  agents_.erase(agent);
}

void BackgroundTracingManagerImpl::AddAgentObserver(
    tracing::TracingAgentObserverManager::AgentObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agent_observers_.insert(observer);

  MaybeConstructPendingAgents();

  for (tracing::mojom::BackgroundTracingAgent* agent : agents_) {
    observer->OnAgentAdded(agent);
  }
}

void BackgroundTracingManagerImpl::RemoveAgentObserver(
    tracing::TracingAgentObserverManager::AgentObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  agent_observers_.erase(observer);

  for (tracing::mojom::BackgroundTracingAgent* agent : agents_) {
    observer->OnAgentRemoved(agent);
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

  if (agent_observers_.empty() && enabled_scenarios_.empty()) {
    return;
  }

  for (auto& pending_agent : pending_agents_) {
    pending_agent.second.set_disconnect_handler(base::OnceClosure());
    BackgroundTracingAgentClientImpl::Create(pending_agent.first,
                                             std::move(pending_agent.second));
  }
  pending_agents_.clear();
}

std::unique_ptr<tracing::BackgroundTracingManager>
CreateBackgroundTracingManager(TracingDelegate* delegate) {
  return std::make_unique<BackgroundTracingManagerImpl>(delegate);
}

}  // namespace content
