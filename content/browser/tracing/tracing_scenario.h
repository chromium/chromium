// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_SCENARIO_H_
#define CONTENT_BROWSER_TRACING_TRACING_SCENARIO_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/token.h"
#include "base/trace_event/trace_config.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/common/content_export.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/scenario_config.gen.h"

namespace content {

// TracingScenario manages triggers and tracing session for a single field
// tracing scenario. TracingScenario allows for multiple scenarios to be enabled
// and watch for rules at once, and is meant to replace
// BackgroundTracingActiveScenario.
// TODO(crbug.com/1418116): Update the comment above once
// BackgroundTracingActiveScenario is deleted.
class CONTENT_EXPORT TracingScenario {
 public:
  enum class State {
    // The scenario is disabled and no rule is installed.
    kDisabled,
    // The scenario is enabled and setup/start rules are installed.
    kEnabled,
    // The tracing session was setup and the scenario is ready to start.
    kSetup,
    // The tracing session is recording.
    kRecording,
    // A stop rule was triggered and the tracing session is stopping.
    kStopping,
    // An upload rule was triggered and the tracing session is finalizing.
    kFinalizing
  };

  // The delegate gets notified of state transitions and receives traces.
  class Delegate {
   public:
    // Called when |scenario| becomes active, i.e. kSetup or kRecoding. Returns
    // true if tracing is allowed to begin.
    virtual bool OnScenarioActive(TracingScenario* scenario) = 0;
    // Called when |scenario| becomes idle again. Returns true if tracing is
    // allowed to finalize.
    virtual bool OnScenarioIdle(TracingScenario* scenario) = 0;
    // Called when |scenario| starts recording a trace.
    virtual void OnScenarioRecording(TracingScenario* scenario) = 0;
    // Called when a trace was collected.
    virtual void SaveTrace(TracingScenario* scenario,
                           const BackgroundTracingRule* triggered_rule,
                           std::string&& serialized_trace) = 0;

   protected:
    ~Delegate() = default;
  };

  static std::unique_ptr<TracingScenario> Create(
      const perfetto::protos::gen::ScenarioConfig& config,
      bool requires_anonymized_data,
      bool enable_package_name_filter,
      Delegate* scenario_delegate);

  virtual ~TracingScenario();

  // Disables an enabled but non-active scenario. Cannot be called after the
  // scenario activates.
  void Disable();
  // Enables a disabled scenario. Cannot be called after the scenario is
  // enabled.
  void Enable();
  // Aborts an active scenario.
  void Abort();

  void GenerateMetadataProto(
      perfetto::protos::pbzero::ChromeMetadataPacket* metadata);

  const std::string& scenario_name() const { return scenario_name_; }
  State current_state() const { return current_state_; }

  base::Token GetSessionID() const { return session_id_; }

 protected:
  TracingScenario(const perfetto::protos::gen::ScenarioConfig& config,
                  Delegate* scenario_delegate);

  virtual std::unique_ptr<perfetto::TracingSession> CreateTracingSession();

 private:
  // Helper deleter to automatically clear on-error callback from
  // perfetto::TracingSession. Without clearing the callback, it is
  // invoked whenever a session is deleted.
  struct TracingSessionDeleter {
    TracingSessionDeleter() = default;
    // NOLINTNEXTLINE(google-explicit-constructor)
    TracingSessionDeleter(std::default_delete<perfetto::TracingSession>) {}
    void operator()(perfetto::TracingSession* ptr) const;
  };
  using TracingSession =
      std::unique_ptr<perfetto::TracingSession, TracingSessionDeleter>;
  class TraceReader;

  bool Initialize(bool requires_anonymized_data,
                  bool enable_package_name_filter);

  void SetupTracingSession();
  void OnTracingError(perfetto::TracingError error);
  void OnTracingStop();
  void OnTracingStart();
  void OnFinalizingDone(std::string&& serialized_trace,
                        TracingSession tracing_session,
                        const BackgroundTracingRule* triggered_rule);

  bool OnSetupTrigger(const BackgroundTracingRule* rule);
  bool OnStartTrigger(const BackgroundTracingRule* rule);
  bool OnStopTrigger(const BackgroundTracingRule* rule);
  bool OnUploadTrigger(const BackgroundTracingRule* rule);

  base::WeakPtr<TracingScenario> GetWeakPtr();
  void SetState(State new_state);

  State current_state_ = State::kDisabled;
  std::vector<std::unique_ptr<BackgroundTracingRule>> setup_rules_;
  std::vector<std::unique_ptr<BackgroundTracingRule>> start_rules_;
  std::vector<std::unique_ptr<BackgroundTracingRule>> stop_rules_;
  std::vector<std::unique_ptr<BackgroundTracingRule>> upload_rules_;

  std::string scenario_name_;
  perfetto::TraceConfig trace_config_;
  raw_ptr<Delegate> scenario_delegate_;
  TracingSession tracing_session_;
  base::Token session_id_;
  raw_ptr<const BackgroundTracingRule> triggered_rule_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<TracingScenario> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_SCENARIO_H_
