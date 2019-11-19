// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_ACTIVE_SCENARIO_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_ACTIVE_SCENARIO_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/background_tracing_manager.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace content {

class BackgroundTracingConfigImpl;

class BackgroundTracingActiveScenario {
 public:
  enum class State { kIdle, kTracing, kFinalizing, kUploading, kAborted };
  class TracingSession;

  BackgroundTracingActiveScenario(
      std::unique_ptr<BackgroundTracingConfigImpl> config,
      BackgroundTracingManager::ReceiveCallback receive_callback,
      base::OnceClosure on_aborted_callback);
  virtual ~BackgroundTracingActiveScenario();

  void StartTracingIfConfigNeedsIt();
  void AbortScenario();

  CONTENT_EXPORT const BackgroundTracingConfigImpl* GetConfig() const;
  void GenerateMetadataDict(base::DictionaryValue* metadata_dict);
  void GenerateMetadataProto(
      perfetto::protos::pbzero::ChromeMetadataPacket* metadata);
  State state() const { return scenario_state_; }
  base::WeakPtr<BackgroundTracingActiveScenario> GetWeakPtr();

  void TriggerNamedEvent(
      BackgroundTracingManager::TriggerHandle handle,
      BackgroundTracingManager::StartedFinalizingCallback callback);
  void OnHistogramTrigger(const std::string& histogram_name);
  void OnRuleTriggered(
      const BackgroundTracingRule* triggered_rule,
      BackgroundTracingManager::StartedFinalizingCallback callback);

  // Called by LegacyTracingSession when the final trace data is ready.
  void OnJSONDataComplete(std::unique_ptr<std::string>);
  // Called by the PerfettoTracingSession when the proto trace is ready.
  void OnProtoDataComplete(std::unique_ptr<std::string> proto_trace);

  // Called when the finalized trace data has been uploaded/transferred away
  // from the background tracing system.
  void OnFinalizeComplete(bool success);

  // For testing
  CONTENT_EXPORT void FireTimerForTesting();
  CONTENT_EXPORT void SetRuleTriggeredCallbackForTesting(
      const base::RepeatingClosure& callback);

  size_t GetTraceUploadLimitKb() const;

 private:
  bool StartTracing();
  void BeginFinalizing(
      BackgroundTracingManager::StartedFinalizingCallback callback);

  BackgroundTracingRule* GetRuleAbleToTriggerTracing(
      const std::string& trigger_name);

  void SetState(State new_state);

  std::unique_ptr<TracingSession> tracing_session_;
  std::unique_ptr<BackgroundTracingConfigImpl> config_;
  // Owned by |config_|.
  const BackgroundTracingRule* last_triggered_rule_ = nullptr;
  State scenario_state_ = State::kIdle;
  base::RepeatingClosure rule_triggered_callback_for_testing_;
  BackgroundTracingManager::ReceiveCallback receive_callback_;
  BackgroundTracingManager::TriggerHandle triggered_named_event_handle_ = -1;
  base::OnceClosure on_aborted_callback_;
  base::OnceClosure started_finalizing_closure_;

  class TracingTimer;
  std::unique_ptr<TracingTimer> tracing_timer_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<BackgroundTracingActiveScenario> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(BackgroundTracingActiveScenario);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_ACTIVE_SCENARIO_H_
