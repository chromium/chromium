// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_SCENARIO_H_
#define CONTENT_BROWSER_TRACING_TRACING_SCENARIO_H_

#include "base/cancelable_callback.h"
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

class CONTENT_EXPORT TracingScenarioBase {
 public:
  virtual ~TracingScenarioBase();

  // Disables a scenario.
  virtual void Disable();
  // Enables a disabled scenario.
  virtual void Enable();

  const std::string& scenario_name() const { return scenario_name_; }

 protected:
  explicit TracingScenarioBase(std::string scenario_name);

  virtual bool OnStartTrigger(const BackgroundTracingRule* rule) = 0;
  virtual bool OnStopTrigger(const BackgroundTracingRule* rule) = 0;
  virtual bool OnUploadTrigger(const BackgroundTracingRule* rule) = 0;

  uint32_t TriggerNameHash(const BackgroundTracingRule* triggered_rule) const;

  std::vector<std::unique_ptr<BackgroundTracingRule>> start_rules_;
  std::vector<std::unique_ptr<BackgroundTracingRule>> stop_rules_;
  std::vector<std::unique_ptr<BackgroundTracingRule>> upload_rules_;

  std::string scenario_name_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

// NestedTracingScenario manages triggers for a single nested tracing
// scenario. Unlike TracingScenario below, it doesn't manage a tracing
// session, but inherits from the parent's session instead.
class CONTENT_EXPORT NestedTracingScenario : public TracingScenarioBase {
 public:
  enum class State {
    // The scenario is disabled and no rule is installed.
    kDisabled,
    // The scenario is enabled, start rules are installed and
    // OnNestedScenarioStart() is called.
    kEnabled,
    // The tracing session is active and stop/upload rules are installed.
    kActive,
    // A stop rule was triggered and only upload rules are installed.
    // After stopping, the nested scenario becomes kDisabled and
    // OnNestedScenarioStop() is called.
    kStopping,
  };

  // The delegate gets notified of state transitions and receives traces.
  class Delegate {
   public:
    // Called when a start rule is triggered and |scenario| becomes kActive.
    virtual void OnNestedScenarioStart(
        NestedTracingScenario* active_scenario) = 0;
    // Called when a stop rule is triggered and |scenario| becomes kStopping.
    // Disable() is expected to be called shortly after.
    virtual void OnNestedScenarioStop(NestedTracingScenario* idle_scenario) = 0;
    // Called when an upload rule is triggered and |scenario| becomes kDisabled.
    virtual void OnNestedScenarioUpload(
        NestedTracingScenario* scenario,
        const BackgroundTracingRule* triggered_rule) = 0;

   protected:
    ~Delegate() = default;
  };

  static std::unique_ptr<NestedTracingScenario> Create(
      const perfetto::protos::gen::NestedScenarioConfig& config,
      Delegate* scenario_delegate);

  ~NestedTracingScenario() override;

  // Disables a scenario.
  void Disable() override;
  // Enables a disabled scenario. Cannot be called after the scenario is
  // enabled.
  void Enable() override;
  // Request to stop an active scenario. Upload rules are still active until
  // Disable() is called.
  void Stop();

  State current_state() const { return current_state_; }

 protected:
  NestedTracingScenario(
      const perfetto::protos::gen::NestedScenarioConfig& config,
      Delegate* scenario_delegate);

  bool Initialize(const perfetto::protos::gen::NestedScenarioConfig& config);

 private:
  bool OnStartTrigger(const BackgroundTracingRule* rule) override;
  bool OnStopTrigger(const BackgroundTracingRule* rule) override;
  bool OnUploadTrigger(const BackgroundTracingRule* rule) override;

  void SetState(State new_state);

  State current_state_ = State::kDisabled;
  raw_ptr<Delegate> scenario_delegate_;
};

// TracingScenario manages triggers and tracing session for a single field
// tracing scenario. TracingScenario allows for multiple scenarios to be enabled
// and watch for rules at once, and is meant to replace
// BackgroundTracingActiveScenario.
// TODO(crbug.com/40257548): Update the comment above once
// BackgroundTracingActiveScenario is deleted.
class CONTENT_EXPORT TracingScenario : public TracingScenarioBase,
                                       public NestedTracingScenario::Delegate {
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
                           base::Token trace_uuid,
                           const BackgroundTracingRule* triggered_rule,
                           std::string&& serialized_trace) = 0;

   protected:
    ~Delegate() = default;
  };

  static std::unique_ptr<TracingScenario> Create(
      const perfetto::protos::gen::ScenarioConfig& config,
      bool enable_privacy_filter,
      bool enable_package_name_filter,
      bool request_startup_tracing,
      Delegate* scenario_delegate);

  ~TracingScenario() override;

  // Disables an enabled but non-active scenario. Cannot be called after the
  // scenario activates.
  void Disable() override;
  // Enables a disabled scenario. Cannot be called after the scenario is
  // enabled.
  void Enable() override;
  // Aborts an active scenario.
  void Abort();

  void GenerateMetadataProto(
      perfetto::protos::pbzero::ChromeMetadataPacket* metadata);

  State current_state() const { return current_state_; }
  bool privacy_filter_enabled() const { return privacy_filtering_enabled_; }
  std::string config_hash() const { return config_hash_; }

  base::Token GetSessionID() const { return session_id_; }

 protected:
  TracingScenario(const perfetto::protos::gen::ScenarioConfig& config,
                  Delegate* scenario_delegate,
                  bool enable_privacy_filter,
                  bool request_startup_tracing);

  bool Initialize(const perfetto::protos::gen::ScenarioConfig& config,
                  bool enable_package_name_filter);

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

  void SetupTracingSession();
  void OnTracingError(perfetto::TracingError error);
  void OnTracingStop();
  void OnTracingStart();
  void OnFinalizingDone(base::Token trace_uuid,
                        std::string&& serialized_trace,
                        TracingSession tracing_session,
                        const BackgroundTracingRule* triggered_rule);
  void DisableNestedScenarios();

  // NestedTracingScenario::Delegate:
  // When called, the base scenario stop rules are uninstalled and other
  // nested scenarios are disabled.
  void OnNestedScenarioStart(NestedTracingScenario* scenario) override;
  // When called, the base scenario remains active and becomes the leaf; stop
  // rules are installed again and all nested scenarios are enabled.
  void OnNestedScenarioStop(NestedTracingScenario* scenario) override;
  // When called, all rules are uinstalled and the tracing session is
  // stopped and finalized.
  void OnNestedScenarioUpload(
      NestedTracingScenario* scenario,
      const BackgroundTracingRule* triggered_rule) override;

  bool OnSetupTrigger(const BackgroundTracingRule* rule);
  bool OnStartTrigger(const BackgroundTracingRule* rule) override;
  bool OnStopTrigger(const BackgroundTracingRule* rule) override;
  bool OnUploadTrigger(const BackgroundTracingRule* rule) override;

  base::WeakPtr<TracingScenario> GetWeakPtr();
  void SetState(State new_state);

  const std::string config_hash_;
  const bool privacy_filtering_enabled_;
  const bool request_startup_tracing_;
  State current_state_ = State::kDisabled;
  std::vector<std::unique_ptr<BackgroundTracingRule>> setup_rules_;

  std::vector<std::unique_ptr<NestedTracingScenario>> nested_scenarios_;
  raw_ptr<NestedTracingScenario> active_scenario_{nullptr};
  base::CancelableOnceClosure on_nested_stopped_;

  perfetto::TraceConfig trace_config_;
  raw_ptr<Delegate> scenario_delegate_;
  TracingSession tracing_session_;
  base::Token session_id_;
  raw_ptr<const BackgroundTracingRule> triggered_rule_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<TracingScenario> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_SCENARIO_H_
