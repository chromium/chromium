// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_scenario.h"

#include <memory>
#include <utility>

#include "base/hash/md5.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/token.h"
#include "base/tracing/trace_time.h"
#include "components/variations/hashing.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/triggers_data_source.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"

namespace content {

namespace {

constexpr uint32_t kStartupTracingTimeoutMs = 30 * 1000;  // 30 sec

}  // namespace

void TracingScenario::TracingSessionDeleter::operator()(
    perfetto::TracingSession* ptr) const {
  ptr->SetOnErrorCallback({});
  delete ptr;
}

class TracingScenario::TraceReader
    : public base::RefCountedThreadSafe<TraceReader> {
 public:
  explicit TraceReader(TracingSession tracing_session, base::Token trace_uuid)
      : tracing_session(std::move(tracing_session)), trace_uuid(trace_uuid) {}

  TracingSession tracing_session;
  base::Token trace_uuid;
  std::string serialized_trace;

 private:
  friend class base::RefCountedThreadSafe<TraceReader>;

  ~TraceReader() = default;
};

using Metrics = BackgroundTracingManagerImpl::Metrics;

TracingScenarioBase::~TracingScenarioBase() = default;

void TracingScenarioBase::Disable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& rule : start_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : upload_rules_) {
    rule->Uninstall();
  }
}

void TracingScenarioBase::Enable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& rule : start_rules_) {
    rule->Install(base::BindRepeating(&TracingScenarioBase::OnStartTrigger,
                                      base::Unretained(this)));
  }
}

uint32_t TracingScenarioBase::TriggerNameHash(
    const BackgroundTracingRule* triggered_rule) const {
  return variations::HashName(
      base::StrCat({scenario_name(), ".", triggered_rule->rule_id()}));
}

TracingScenarioBase::TracingScenarioBase(std::string scenario_name)
    : scenario_name_(std::move(scenario_name)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

// static
std::unique_ptr<NestedTracingScenario> NestedTracingScenario::Create(
    const perfetto::protos::gen::NestedScenarioConfig& config,
    Delegate* scenario_delegate) {
  auto scenario =
      base::WrapUnique(new NestedTracingScenario(config, scenario_delegate));
  if (!scenario->Initialize(config)) {
    return nullptr;
  }
  return scenario;
}

NestedTracingScenario::NestedTracingScenario(
    const perfetto::protos::gen::NestedScenarioConfig& config,
    Delegate* scenario_delegate)
    : TracingScenarioBase(config.scenario_name()),
      scenario_delegate_(scenario_delegate) {}

NestedTracingScenario::~NestedTracingScenario() = default;

void NestedTracingScenario::Disable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetState(State::kDisabled);
  TracingScenarioBase::Disable();
}

void NestedTracingScenario::Enable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(current_state_, State::kDisabled);
  SetState(State::kEnabled);
  TracingScenarioBase::Enable();
}

void NestedTracingScenario::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(current_state_ == State::kActive || current_state_ == State::kStopping)
      << static_cast<int>(current_state_);
  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  SetState(State::kStopping);
}

bool NestedTracingScenario::Initialize(
    const perfetto::protos::gen::NestedScenarioConfig& config) {
  return BackgroundTracingRule::Append(config.start_rules(), start_rules_) &&
         BackgroundTracingRule::Append(config.stop_rules(), stop_rules_) &&
         BackgroundTracingRule::Append(config.upload_rules(), upload_rules_);
}

bool NestedTracingScenario::OnStartTrigger(
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (current_state() != State::kEnabled) {
    return false;
  }
  tracing::TriggersDataSource::EmitTrigger(triggered_rule->rule_id());
  base::UmaHistogramSparse("Tracing.Background.Scenario.Trigger.Start",
                           TriggerNameHash(triggered_rule));
  for (auto& rule : start_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : stop_rules_) {
    rule->Install(base::BindRepeating(&NestedTracingScenario::OnStopTrigger,
                                      base::Unretained(this)));
  }
  for (auto& rule : upload_rules_) {
    rule->Install(base::BindRepeating(&NestedTracingScenario::OnUploadTrigger,
                                      base::Unretained(this)));
  }
  scenario_delegate_->OnNestedScenarioStart(this);
  SetState(State::kActive);
  return true;
}

bool NestedTracingScenario::OnStopTrigger(
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracing::TriggersDataSource::EmitTrigger(triggered_rule->rule_id());
  base::UmaHistogramSparse("Tracing.Background.Scenario.Trigger.Stop",
                           TriggerNameHash(triggered_rule));
  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  SetState(State::kStopping);
  scenario_delegate_->OnNestedScenarioStop(this);
  return true;
}

bool NestedTracingScenario::OnUploadTrigger(
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : upload_rules_) {
    rule->Uninstall();
  }
  SetState(State::kDisabled);
  scenario_delegate_->OnNestedScenarioUpload(this, triggered_rule);
  return true;
}

void NestedTracingScenario::SetState(State new_state) {
  current_state_ = new_state;
}

// static
std::unique_ptr<TracingScenario> TracingScenario::Create(
    const perfetto::protos::gen::ScenarioConfig& config,
    bool enable_privacy_filter,
    bool enable_package_name_filter,
    bool request_startup_tracing,
    Delegate* scenario_delegate) {
  auto scenario = base::WrapUnique(
      new TracingScenario(config, scenario_delegate, enable_privacy_filter,
                          request_startup_tracing));
  if (!scenario->Initialize(config, enable_package_name_filter)) {
    return nullptr;
  }
  return scenario;
}

TracingScenario::TracingScenario(
    const perfetto::protos::gen::ScenarioConfig& config,
    Delegate* scenario_delegate,
    bool enable_privacy_filter,
    bool request_startup_tracing)
    : TracingScenarioBase(config.scenario_name()),
      config_hash_(base::MD5String(config.SerializeAsString())),
      privacy_filtering_enabled_(enable_privacy_filter),
      request_startup_tracing_(request_startup_tracing),
      trace_config_(config.trace_config()),
      scenario_delegate_(scenario_delegate) {}

TracingScenario::~TracingScenario() = default;

bool TracingScenario::Initialize(
    const perfetto::protos::gen::ScenarioConfig& config,
    bool enable_package_name_filter) {
  if (!tracing::AdaptPerfettoConfigForChrome(
          &trace_config_, privacy_filtering_enabled_,
          enable_package_name_filter,
          perfetto::protos::gen::ChromeConfig::BACKGROUND)) {
    return false;
  }
  for (const auto& nested_config : config.nested_scenarios()) {
    auto nested_scenario = NestedTracingScenario::Create(nested_config, this);
    if (!nested_scenario) {
      return false;
    }
    nested_scenarios_.push_back(std::move(nested_scenario));
  }
  return BackgroundTracingRule::Append(config.start_rules(), start_rules_) &&
         BackgroundTracingRule::Append(config.stop_rules(), stop_rules_) &&
         BackgroundTracingRule::Append(config.upload_rules(), upload_rules_) &&
         BackgroundTracingRule::Append(config.setup_rules(), setup_rules_);
}

void TracingScenario::Disable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(current_state_, State::kEnabled);
  SetState(State::kDisabled);
  for (auto& rule : setup_rules_) {
    rule->Uninstall();
  }
  TracingScenarioBase::Disable();
}

void TracingScenario::Enable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(current_state_, State::kDisabled);
  SetState(State::kEnabled);
  for (auto& rule : setup_rules_) {
    rule->Install(base::BindRepeating(&TracingScenario::OnSetupTrigger,
                                      base::Unretained(this)));
  }
  TracingScenarioBase::Enable();
}

void TracingScenario::Abort() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TracingScenarioBase::Disable();
  DisableNestedScenarios();
  SetState(State::kStopping);
  tracing_session_->Stop();
}

void TracingScenario::GenerateMetadataProto(
    perfetto::protos::pbzero::ChromeMetadataPacket* metadata) {
  auto* background_tracing_metadata =
      metadata->set_background_tracing_metadata();

  uint32_t scenario_name_hash = variations::HashName(scenario_name());
  background_tracing_metadata->set_scenario_name_hash(scenario_name_hash);

  if (triggered_rule_) {
    auto* triggered_rule = background_tracing_metadata->set_triggered_rule();
    triggered_rule_->GenerateMetadataProto(triggered_rule);
  }
}

std::unique_ptr<perfetto::TracingSession>
TracingScenario::CreateTracingSession() {
  return perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);
}

void TracingScenario::SetupTracingSession() {
  DCHECK(!tracing_session_);
  tracing_session_ = CreateTracingSession();
  session_id_ = base::Token::CreateRandom();
  trace_config_.set_trace_uuid_msb(session_id_.high());
  trace_config_.set_trace_uuid_lsb(session_id_.low());
  tracing_session_->Setup(trace_config_);
  tracing_session_->SetOnStartCallback([task_runner = task_runner_,
                                        weak_ptr = GetWeakPtr()]() {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&TracingScenario::OnTracingStart, weak_ptr));
  });
  tracing_session_->SetOnErrorCallback(
      [task_runner = task_runner_,
       weak_ptr = GetWeakPtr()](perfetto::TracingError error) {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(&TracingScenario::OnTracingError, weak_ptr, error));
      });
}

void TracingScenario::OnNestedScenarioStart(
    NestedTracingScenario* active_scenario) {
  CHECK_EQ(active_scenario_, nullptr);
  active_scenario_ = active_scenario;
  // Other nested scenarios are disabled and stop rules are uninstalled.
  for (auto& scenario : nested_scenarios_) {
    if (scenario.get() == active_scenario_) {
      continue;
    }
    scenario->Disable();
  }
  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  // If in `kSetup`, the tracing session is started.
  if (current_state() == State::kSetup) {
    OnStartTrigger(nullptr);
  }
}

void TracingScenario::OnNestedScenarioStop(
    NestedTracingScenario* nested_scenario) {
  CHECK_EQ(active_scenario_, nested_scenario);
  for (auto& rule : stop_rules_) {
    rule->Install(base::BindRepeating(&TracingScenario::OnStopTrigger,
                                      base::Unretained(this)));
  }
  // Stop the scenario asynchronously in case an upload trigger is triggered in
  // the same task.
  on_nested_stopped_.Reset(base::BindOnce(
      [](TracingScenario* self, NestedTracingScenario* nested_scenario) {
        CHECK_EQ(nested_scenario->current_state(),
                 NestedTracingScenario::State::kStopping);
        CHECK_EQ(self->current_state_, State::kRecording);
        CHECK_EQ(self->active_scenario_, nested_scenario);
        nested_scenario->Disable();
        self->active_scenario_ = nullptr;
        // All nested scenarios are re-enabled.
        for (auto& scenario : self->nested_scenarios_) {
          scenario->Enable();
        }
      },
      base::Unretained(this), nested_scenario));
  task_runner_->PostTask(FROM_HERE, on_nested_stopped_.callback());
}

void TracingScenario::OnNestedScenarioUpload(
    NestedTracingScenario* scenario,
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_EQ(active_scenario_, scenario);
  active_scenario_ = nullptr;
  OnUploadTrigger(triggered_rule);
}

bool TracingScenario::OnSetupTrigger(
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!scenario_delegate_->OnScenarioActive(this)) {
    return false;
  }

  for (auto& rule : setup_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : stop_rules_) {
    rule->Install(base::BindRepeating(&TracingScenario::OnStopTrigger,
                                      base::Unretained(this)));
  }
  for (auto& rule : upload_rules_) {
    rule->Install(base::BindRepeating(&TracingScenario::OnUploadTrigger,
                                      base::Unretained(this)));
  }
  for (auto& scenario : nested_scenarios_) {
    scenario->Enable();
  }
  SetState(State::kSetup);
  SetupTracingSession();
  return true;
}

bool TracingScenario::OnStartTrigger(
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (current_state() == State::kEnabled) {
    // Move to setup before starting the session below.
    if (!OnSetupTrigger(triggered_rule)) {
      return false;
    }
  } else if (current_state() != State::kSetup) {
    return false;
  }

  for (auto& rule : start_rules_) {
    rule->Uninstall();
  }

  SetState(State::kRecording);

  if (request_startup_tracing_) {
    perfetto::Tracing::SetupStartupTracingOpts opts;
    opts.timeout_ms = kStartupTracingTimeoutMs;
    opts.backend = perfetto::kCustomBackend;
    tracing::PerfettoTracedProcess::Get()->RequestStartupTracing(trace_config_,
                                                                 opts);
  }

  tracing_session_->SetOnStopCallback([task_runner = task_runner_,
                                       weak_ptr = GetWeakPtr()]() {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&TracingScenario::OnTracingStop, weak_ptr));
  });
  tracing_session_->Start();
  if (triggered_rule) {
    tracing::TriggersDataSource::EmitTrigger(triggered_rule->rule_id());
    base::UmaHistogramSparse("Tracing.Background.Scenario.Trigger.Start",
                             TriggerNameHash(triggered_rule));
  }
  return true;
}

bool TracingScenario::OnStopTrigger(
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tracing::TriggersDataSource::EmitTrigger(triggered_rule->rule_id());
  base::UmaHistogramSparse("Tracing.Background.Scenario.Trigger.Stop",
                           TriggerNameHash(triggered_rule));
  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  if (active_scenario_) {
    on_nested_stopped_.Cancel();
    active_scenario_->Stop();
  } else {
    for (auto& nested_scenario : nested_scenarios_) {
      nested_scenario->Disable();
    }
  }
  if (current_state_ == State::kSetup) {
    CHECK_EQ(nullptr, active_scenario_);
    // Tear down the session since we haven't been tracing yet.
    for (auto& rule : upload_rules_) {
      rule->Uninstall();
    }
    for (auto& rule : start_rules_) {
      rule->Uninstall();
    }
    tracing_session_.reset();
    SetState(State::kDisabled);
    scenario_delegate_->OnScenarioIdle(this);
    return true;
  }
  tracing_session_->Stop();
  SetState(State::kStopping);
  return true;
}

bool TracingScenario::OnUploadTrigger(
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tracing::TriggersDataSource::EmitTrigger(triggered_rule->rule_id());
  base::UmaHistogramSparse("Tracing.Background.Scenario.Trigger.Upload",
                           TriggerNameHash(triggered_rule));
  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : upload_rules_) {
    rule->Uninstall();
  }
  DisableNestedScenarios();
  // Setup is ignored.
  if (current_state_ == State::kSetup) {
    for (auto& rule : start_rules_) {
      rule->Uninstall();
    }
    tracing_session_.reset();
    SetState(State::kDisabled);
    scenario_delegate_->OnScenarioIdle(this);
    return true;
  }
  CHECK(current_state_ == State::kRecording ||
        current_state_ == State::kStopping)
      << static_cast<int>(current_state_);
  triggered_rule_ = triggered_rule;
  if (current_state_ != State::kStopping) {
    tracing_session_->Stop();
  }
  SetState(State::kFinalizing);
  return true;
}

void TracingScenario::OnTracingError(perfetto::TracingError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!tracing_session_) {
    CHECK(current_state_ == State::kDisabled ||
          current_state_ == State::kEnabled)
        << static_cast<int>(current_state_);
    return;
  }
  for (auto& rule : start_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : upload_rules_) {
    rule->Uninstall();
  }
  DisableNestedScenarios();
  SetState(State::kStopping);
  tracing_session_->Stop();
  // TODO(crbug.com/40257548): Consider reporting |error|.
}

void TracingScenario::OnTracingStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scenario_delegate_->OnScenarioRecording(this);
}

void TracingScenario::OnTracingStop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (current_state_ != State::kStopping &&
      current_state_ != State::kFinalizing) {
    // Tracing was stopped internally.
    CHECK(current_state_ == State::kSetup ||
          current_state_ == State::kRecording)
        << static_cast<int>(current_state_);
    for (auto& rule : start_rules_) {
      rule->Uninstall();
    }
    for (auto& rule : stop_rules_) {
      rule->Uninstall();
    }
  }
  for (auto& rule : upload_rules_) {
    rule->Uninstall();
  }
  DisableNestedScenarios();
  bool should_upload = (current_state_ == State::kFinalizing);
  auto tracing_session = std::move(tracing_session_);
  SetState(State::kDisabled);
  if (!scenario_delegate_->OnScenarioIdle(this)) {
    should_upload = false;
  }
  if (!should_upload) {
    tracing_session.reset();
    return;
  }
  DCHECK(triggered_rule_);
  auto reader = base::MakeRefCounted<TraceReader>(std::move(tracing_session),
                                                  session_id_);
  reader->tracing_session->ReadTrace(
      [task_runner = task_runner_, weak_ptr = GetWeakPtr(), reader,
       triggered_rule = std::move(triggered_rule_).get()](
          perfetto::TracingSession::ReadTraceCallbackArgs args) mutable {
        if (args.size) {
          reader->serialized_trace.append(args.data, args.size);
        }
        if (!args.has_more) {
          task_runner->PostTask(
              FROM_HERE, base::BindOnce(&TracingScenario::OnFinalizingDone,
                                        weak_ptr, reader->trace_uuid,
                                        std::move(reader->serialized_trace),
                                        std::move(reader->tracing_session),
                                        triggered_rule));
        }
      });
}

void TracingScenario::OnFinalizingDone(
    base::Token trace_uuid,
    std::string&& serialized_trace,
    TracingSession tracing_session,
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tracing_session.reset();
  scenario_delegate_->SaveTrace(this, trace_uuid, triggered_rule,
                                std::move(serialized_trace));
}

void TracingScenario::DisableNestedScenarios() {
  if (active_scenario_) {
    CHECK(current_state_ == State::kRecording ||
          current_state_ == State::kStopping)
        << static_cast<int>(current_state_);
    on_nested_stopped_.Cancel();
    active_scenario_->Disable();
    active_scenario_ = nullptr;
  } else if (current_state_ == State::kRecording ||
             current_state_ == State::kSetup) {
    for (auto& nested_scenario : nested_scenarios_) {
      nested_scenario->Disable();
    }
  }
}

void TracingScenario::SetState(State new_state) {
  if (new_state == State::kEnabled || new_state == State::kDisabled) {
    CHECK_EQ(nullptr, tracing_session_);
    CHECK_EQ(nullptr, active_scenario_);
    for (auto& scenario : nested_scenarios_) {
      CHECK_EQ(NestedTracingScenario::State::kDisabled,
               scenario->current_state());
    }
  }
  current_state_ = new_state;
}

base::WeakPtr<TracingScenario> TracingScenario::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
