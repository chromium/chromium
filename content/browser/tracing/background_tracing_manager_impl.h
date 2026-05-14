// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "content/browser/tracing/traces_internals/traces_internals.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_manager.h"
#include "services/tracing/public/cpp/background_tracing/tracing_agent_observer_manager.h"
#include "services/tracing/public/mojom/background_tracing_agent.mojom.h"

namespace tracing::mojom {
class BackgroundTracingAgent;
class BackgroundTracingAgentProvider;
}  // namespace tracing::mojom

namespace content {
namespace mojom {
class ChildProcess;
}  // namespace mojom
class TracingDelegate;

class BackgroundTracingManagerImpl
    : public tracing::BackgroundTracingManager,
      public tracing::TracingAgentObserverManager {
 public:
  // Delegate to store and read application preferences for startup tracing, to
  // isolate the feature for testing.
  class PreferenceManager {
   public:
    virtual bool GetBackgroundStartupTracingEnabled() const = 0;
    virtual ~PreferenceManager() = default;
  };

  CONTENT_EXPORT static BackgroundTracingManagerImpl& GetInstance();

  explicit CONTENT_EXPORT BackgroundTracingManagerImpl(
      TracingDelegate* delegate);
  ~BackgroundTracingManagerImpl() override;

  BackgroundTracingManagerImpl(const BackgroundTracingManagerImpl&) = delete;
  BackgroundTracingManagerImpl& operator=(const BackgroundTracingManagerImpl&) =
      delete;

  // Callable from any thread.
  static void ActivateForProcess(int child_process_id,
                                 mojom::ChildProcess* child_process);

  // tracing::BackgroundTracingManager implementation:
  bool IsRecordingAllowed(bool privacy_filter_enabled,
                          base::TimeTicks scenario_start_time) override;
  bool ShouldSaveUnuploadedTrace() override;
  std::string RecordSerializedSystemProfileMetrics() override;
  std::optional<base::FilePath> GetLocalTracesDirectory() override;
  bool GetBackgroundStartupTracingEnabled() const override;

  // Returns the list of scenario hashes and names that were saved,
  // whether or not enabled.
  CONTENT_EXPORT std::vector<traces_internals::mojom::ScenarioPtr>
  GetAllScenarios() const;

  // Returns the list of scenario hashes that are currently enabled. These are
  // either all preset scenarios or all field scenarios.

  // Add/remove Agent{Observer}.
  void AddAgent(tracing::mojom::BackgroundTracingAgent* agent);
  void RemoveAgent(tracing::mojom::BackgroundTracingAgent* agent);
  void AddAgentObserver(
      tracing::TracingAgentObserverManager::AgentObserver* observer) override;
  void RemoveAgentObserver(
      tracing::TracingAgentObserverManager::AgentObserver* observer) override;

  // For tests
  CONTENT_EXPORT void SetPreferenceManagerForTesting(
      std::unique_ptr<PreferenceManager> preferences);

  void GenerateMetadataProto(
      perfetto::protos::pbzero::ChromeMetadataPacket* metadata,
      bool privacy_filtering_enabled);

 private:
  static void AddPendingAgent(
      int child_process_id,
      mojo::PendingRemote<tracing::mojom::BackgroundTracingAgentProvider>
          provider);
  static void ClearPendingAgent(int child_process_id);
  void MaybeConstructPendingAgents() override;

  raw_ptr<TracingDelegate> delegate_;
  std::unique_ptr<tracing::BackgroundTracingStateManager> state_manager_;
  std::unique_ptr<PreferenceManager> preferences_;

  std::set<raw_ptr<tracing::mojom::BackgroundTracingAgent, SetExperimental>>
      agents_;
  std::set<raw_ptr<tracing::TracingAgentObserverManager::AgentObserver,
                   SetExperimental>>
      agent_observers_;

  std::map<int, mojo::Remote<tracing::mojom::BackgroundTracingAgentProvider>>
      pending_agents_;

  base::WeakPtrFactory<BackgroundTracingManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_MANAGER_IMPL_H_
