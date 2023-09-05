// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_ACTIVE_SCENARIO_H_
#define CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_ACTIVE_SCENARIO_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_tracing_manager.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

namespace content {

class BackgroundTracingConfigImpl;
class BackgroundTracingRule;
class TracingDelegate;

class BackgroundTracingActiveScenario {
 public:
  enum class State { kIdle, kTracing, kFinalizing, kUploading, kAborted };
  class TracingSession;

  BackgroundTracingActiveScenario(
      std::unique_ptr<BackgroundTracingConfigImpl> config,
      TracingDelegate* delegate,
      base::OnceClosure on_aborted_callback);

  BackgroundTracingActiveScenario(const BackgroundTracingActiveScenario&) =
      delete;
  BackgroundTracingActiveScenario& operator=(
      const BackgroundTracingActiveScenario&) = delete;

  virtual ~BackgroundTracingActiveScenario();

  void StartTracingIfConfigNeedsIt();
  void AbortScenario();

  CONTENT_EXPORT const BackgroundTracingConfigImpl* GetConfig() const;
  void GenerateMetadataProto(
      perfetto::protos::pbzero::ChromeMetadataPacket* metadata);
  State state() const { return scenario_state_; }
  base::WeakPtr<BackgroundTracingActiveScenario> GetWeakPtr();

  bool OnRuleTriggered(const BackgroundTracingRule* triggered_rule);

  // Called by TracingSession when the final trace data is ready for proto
  // traces.
  void OnProtoDataComplete(std::string&& serialized_trace);

  // For testing
  CONTENT_EXPORT void FireTimerForTesting();
  CONTENT_EXPORT void SetRuleTriggeredCallbackForTesting(
      const base::RepeatingClosure& callback);

 private:
  bool StartTracing();
  void BeginFinalizing();

  void SetState(State new_state);

  std::unique_ptr<TracingSession> tracing_session_;
  std::unique_ptr<BackgroundTracingConfigImpl> config_;
  // Owned by |config_|.
  raw_ptr<const BackgroundTracingRule> last_triggered_rule_ = nullptr;
  State scenario_state_ = State::kIdle;
  base::RepeatingClosure rule_triggered_callback_for_testing_;
  raw_ptr<TracingDelegate> delegate_;
  base::OnceClosure on_aborted_callback_;

  class TracingTimer;
  std::unique_ptr<TracingTimer> tracing_timer_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<BackgroundTracingActiveScenario> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_BACKGROUND_TRACING_ACTIVE_SCENARIO_H_
