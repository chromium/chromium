// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_scenario.h"

#include "base/strings/stringprintf.h"
#include "base/tracing/trace_time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"

namespace content {

TracingScenario::TracingScenario(
    const perfetto::protos::gen::ScenarioConfig& config,
    Delegate* delegate)
    : scenario_name_(config.scenario_name()),
      trace_config_(config.trace_config()),
      delegate_(delegate),
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

void TracingScenario::Disable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(current_state_, State::kEnabled);
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
  DCHECK_EQ(current_state_, State::kDisabled);
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

void TracingScenario::SetupTracingSession() {
  DCHECK(!tracing_session_);
  tracing_session_ = CreateTracingSession();
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

  for (auto& rule : setup_rules_) {
    rule->Uninstall();
  }
  delegate_->OnScenarioActive(this);
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
    OnSetupTrigger(triggered_rule);
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
    delegate_->OnScenarioIdle(this);
    return true;
  }
  SetState(State::kStopping);
  tracing_session_->Stop();
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
    delegate_->OnScenarioIdle(this);
    return true;
  }
  DCHECK(current_state_ == State::kRecording ||
         current_state_ == State::kStopping);
  SetState(State::kFinalizing);
  tracing_session_->Stop();
  return true;
}

void TracingScenario::OnTracingError(perfetto::TracingError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(current_state_ == State::kSetup ||
         current_state_ == State::kRecording ||
         current_state_ == State::kStopping);
  for (auto& rule : start_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : stop_rules_) {
    rule->Uninstall();
  }
  for (auto& rule : upload_rules_) {
    rule->Uninstall();
  }
  tracing_session_.reset();
  SetState(State::kDisabled);
  delegate_->OnScenarioIdle(this);
  // TODO(crbug.com/1418116): Consider reporting |error|.
}

void TracingScenario::OnTracingStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TracingScenario::OnTracingStop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (current_state_ == State::kStopping) {
    for (auto& rule : upload_rules_) {
      rule->Uninstall();
    }
    tracing_session_.reset();
    SetState(State::kDisabled);
    delegate_->OnScenarioIdle(this);
    return;
  }
  DCHECK_EQ(current_state_, State::kFinalizing);
  tracing_session_->ReadTrace(
      [task_runner = task_runner_, weak_ptr = GetWeakPtr(),
       raw_data = std::string()](
          perfetto::TracingSession::ReadTraceCallbackArgs args) mutable {
        if (args.size) {
          raw_data.append(args.data, args.size);
        }
        if (!args.has_more) {
          task_runner->PostTask(
              FROM_HERE, base::BindOnce(&TracingScenario::OnFinalizingDone,
                                        weak_ptr, std::move(raw_data)));
        }
      });
}

void TracingScenario::OnFinalizingDone(std::string trace_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tracing_session_.reset();
  delegate_->SaveTrace(this, std::move(trace_data));
  SetState(State::kDisabled);
  delegate_->OnScenarioIdle(this);
}

void TracingScenario::SetState(State new_state) {
  if (new_state == State::kEnabled || new_state == State::kDisabled) {
    DCHECK_EQ(nullptr, tracing_session_);
  }
  current_state_ = new_state;
}

base::WeakPtr<TracingScenario> TracingScenario::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace content
