// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_scenario.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/stringprintf.h"
#include "base/token.h"
#include "base/tracing/trace_time.h"
#include "components/variations/hashing.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"

namespace content {

void TracingScenario::TracingSessionDeleter::operator()(
    perfetto::TracingSession* ptr) const {
  ptr->SetOnErrorCallback({});
  delete ptr;
}

class TracingScenario::TraceReader
    : public base::RefCountedThreadSafe<TraceReader> {
 public:
  explicit TraceReader(TracingSession tracing_session)
      : tracing_session(std::move(tracing_session)) {}

  TracingSession tracing_session;
  std::string serialized_trace;

 private:
  friend class base::RefCountedThreadSafe<TraceReader>;

  ~TraceReader() = default;
};

using Metrics = BackgroundTracingManagerImpl::Metrics;

// static
std::unique_ptr<TracingScenario> TracingScenario::Create(
    const perfetto::protos::gen::ScenarioConfig& config,
    bool requires_anonymized_data,
    bool enable_package_name_filter,
    Delegate* scenario_delegate) {
  auto scenario =
      base::WrapUnique(new TracingScenario(config, scenario_delegate));
  if (!scenario->Initialize(requires_anonymized_data,
                            enable_package_name_filter)) {
    return nullptr;
  }
  return scenario;
}

TracingScenario::TracingScenario(
    const perfetto::protos::gen::ScenarioConfig& config,
    Delegate* scenario_delegate)
    : scenario_name_(config.scenario_name()),
      trace_config_(config.trace_config()),
      scenario_delegate_(scenario_delegate),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  for (const auto& rule : config.start_rules()) {
    start_rules_.push_back(BackgroundTracingRule::Create(rule));
  }
  for (const auto& rule : config.stop_rules()) {
    stop_rules_.push_back(BackgroundTracingRule::Create(rule));
  }
  for (const auto& rule : config.upload_rules()) {
    upload_rules_.push_back(BackgroundTracingRule::Create(rule));
  }
  for (const auto& rule : config.setup_rules()) {
    setup_rules_.push_back(BackgroundTracingRule::Create(rule));
  }
}

TracingScenario::~TracingScenario() = default;

bool TracingScenario::Initialize(bool requires_anonymized_data,
                                 bool enable_package_name_filter) {
  return tracing::AdaptPerfettoConfigForChrome(
      &trace_config_, requires_anonymized_data, enable_package_name_filter,
      perfetto::protos::gen::ChromeConfig::BACKGROUND);
}

void TracingScenario::Disable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(current_state_, State::kEnabled);
  SetState(State::kDisabled);
  for (auto& rule : start_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : upload_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : setup_rules_) {
    rule->Uninstall();
  }
}

void TracingScenario::Enable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(current_state_, State::kDisabled);
  SetState(State::kEnabled);
  for (auto& rule : start_rules_) {
    rule->Install(base::BindRepeating(&TracingScenario::OnStartTrigger,
                                      base::Unretained(this)));
  }
  for (auto& rule : setup_rules_) {
    rule->Install(base::BindRepeating(&TracingScenario::OnSetupTrigger,
                                      base::Unretained(this)));
  }
}

void TracingScenario::Abort() {
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
  tracing_session_->SetOnStopCallback([task_runner = task_runner_,
                                       weak_ptr = GetWeakPtr()]() {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&TracingScenario::OnTracingStop, weak_ptr));
  });
  tracing_session_->Start();
  return true;
}

bool TracingScenario::OnStopTrigger(
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  if (current_state_ == State::kSetup) {
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

  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : upload_rules_) {
    rule->Uninstall();
  }
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
  SetState(State::kStopping);
  tracing_session_->Stop();
  // TODO(crbug.com/1418116): Consider reporting |error|.
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
  auto reader = base::MakeRefCounted<TraceReader>(std::move(tracing_session));
  reader->tracing_session->ReadTrace(
      [task_runner = task_runner_, weak_ptr = GetWeakPtr(), reader,
       triggered_rule = std::move(triggered_rule_).get()](
          perfetto::TracingSession::ReadTraceCallbackArgs args) mutable {
        if (args.size) {
          reader->serialized_trace.append(args.data, args.size);
        }
        if (!args.has_more) {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(&TracingScenario::OnFinalizingDone, weak_ptr,
                             std::move(reader->serialized_trace),
                             std::move(reader->tracing_session),
                             triggered_rule));
        }
      });
}

void TracingScenario::OnFinalizingDone(
    std::string&& serialized_trace,
    TracingSession tracing_session,
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tracing_session.reset();
  scenario_delegate_->SaveTrace(this, triggered_rule,
                                std::move(serialized_trace));
}

void TracingScenario::SetState(State new_state) {
  if (new_state == State::kEnabled || new_state == State::kDisabled) {
    CHECK_EQ(nullptr, tracing_session_);
  }
  current_state_ = new_state;
}

base::WeakPtr<TracingScenario> TracingScenario::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
