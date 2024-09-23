// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_tracing_active_scenario.h"

#include <set>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_tokenizer.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/variations/hashing.h"
#include "content/browser/tracing/background_tracing_config_impl.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_delegate.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

using base::trace_event::TraceConfig;
using Metrics = content::BackgroundTracingManagerImpl::Metrics;

namespace content {

class BackgroundTracingActiveScenario::TracingTimer {
 public:
  explicit TracingTimer(BackgroundTracingActiveScenario* scenario)
      : scenario_(scenario) {
    DCHECK_NE(scenario->GetConfig()->tracing_mode(),
              BackgroundTracingConfigImpl::SYSTEM);
  }
  ~TracingTimer() = default;

  void StartTimer(base::TimeDelta delay) {
    tracing_timer_.Start(FROM_HERE, delay, this,
                         &TracingTimer::TracingTimerFired);
  }
  void CancelTimer() { tracing_timer_.Stop(); }

  void FireTimerForTesting() {
    CancelTimer();
    TracingTimerFired();
  }

 private:
  void TracingTimerFired() { scenario_->BeginFinalizing(); }

  raw_ptr<BackgroundTracingActiveScenario> scenario_;
  base::OneShotTimer tracing_timer_;
};

class BackgroundTracingActiveScenario::TracingSession {
 public:
  TracingSession(BackgroundTracingActiveScenario* parent_scenario,
                 const TraceConfig& chrome_config,
                 const BackgroundTracingConfigImpl* config)
      : parent_scenario_(parent_scenario) {
    // TODO(khokhlov): Re-enable startup tracing in SDK build. Make sure that
    // startup tracing config exactly matches non-startup tracing config.
    perfetto::TraceConfig perfetto_config;
    perfetto_config.mutable_incremental_state_config()->set_clear_period_ms(
        config->interning_reset_interval_ms());
    base::StringTokenizer data_sources(config->enabled_data_sources(), ",");
    std::set<std::string> data_source_filter;
    while (data_sources.GetNext()) {
      data_source_filter.insert(data_sources.token());
    }
    perfetto_config = tracing::GetPerfettoConfigWithDataSources(
        chrome_config, data_source_filter, config->requires_anonymized_data(),
        /*convert_to_legacy_json=*/false,
        perfetto::protos::gen::ChromeConfig::BACKGROUND);
    tracing_session_ =
        perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);
    tracing_session_->Setup(perfetto_config);

    tracing_session_->SetOnStartCallback([] {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BackgroundTracingManagerImpl::OnStartTracingDone,
              base::Unretained(&BackgroundTracingManagerImpl::GetInstance())));
    });
    tracing_session_->Start();
    // We check IsEnabled() before creating the LegacyTracingSession,
    // so any failures to start tracing at this point would be due to invalid
    // configs which we treat as a failure scenario.
  }

  ~TracingSession() {
    DCHECK(!tracing_session_);
    DCHECK(!TracingControllerImpl::GetInstance()->IsTracing());
  }

  void BeginFinalizing(base::OnceClosure on_success,
                       base::OnceClosure on_failure,
                       bool is_crash_scenario) {
    // If the finalization was already in progress, ignore this call.
    if (!tracing_session_) {
      return;
    }

    if (parent_scenario_->delegate_ &&
        (!parent_scenario_->delegate_->OnBackgroundTracingIdle(
            parent_scenario_->GetConfig()->requires_anonymized_data()))) {
      auto on_failure_cb =
          base::MakeRefCounted<base::RefCountedData<base::OnceClosure>>(
              std::move(on_failure));
      auto tracing_session = TakeTracingSession();
      tracing_session->data->SetOnStopCallback(
          [tracing_session, on_failure_cb] {
            GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                std::move(on_failure_cb->data));
          });
      tracing_session->data->Stop();
      return;
    }

    FinalizeTraceAsProtobuf(std::move(on_success));
    DCHECK(!tracing_session_);
  }

  void AbortScenario(const base::RepeatingClosure& on_abort_callback) {
    if (tracing_session_) {
      auto tracing_session = TakeTracingSession();
      auto on_abort_cb =
          base::MakeRefCounted<base::RefCountedData<base::OnceClosure>>(
              std::move(on_abort_callback));
      tracing_session->data->SetOnStopCallback([on_abort_cb, tracing_session] {
        GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                            std::move(on_abort_cb->data));
      });
      tracing_session->data->Stop();
    } else {
      on_abort_callback.Run();
    }
  }

 private:
  // Wraps the tracing session in a refcounted handle that can be passed through
  // callbacks.
  scoped_refptr<base::RefCountedData<std::unique_ptr<perfetto::TracingSession>>>
  TakeTracingSession() {
    return base::MakeRefCounted<
        base::RefCountedData<std::unique_ptr<perfetto::TracingSession>>>(
        std::move(tracing_session_));
  }

  void FinalizeTraceAsProtobuf(base::OnceClosure on_success) {
    auto tracing_session = TakeTracingSession();
    auto raw_data =
        base::MakeRefCounted<base::RefCountedData<std::string>>(std::string());
    auto parent_scenario = parent_scenario_->GetWeakPtr();
    tracing_session->data->SetOnStopCallback(
        [parent_scenario, tracing_session, raw_data] {
          tracing_session->data->ReadTrace(
              [parent_scenario, tracing_session,
               raw_data](perfetto::TracingSession::ReadTraceCallbackArgs args) {
                if (args.size) {
                  raw_data->data.append(args.data, args.size);
                }
                if (!args.has_more) {
                  GetUIThreadTaskRunner({})->PostTask(
                      FROM_HERE,
                      base::BindOnce(
                          &BackgroundTracingActiveScenario::OnProtoDataComplete,
                          parent_scenario, std::move(raw_data->data)));
                }
              });
        });
    tracing_session->data->Stop();
    if (on_success) {
      std::move(on_success).Run();
    }
  }

  const raw_ptr<BackgroundTracingActiveScenario> parent_scenario_;
  std::unique_ptr<perfetto::TracingSession> tracing_session_;
};

BackgroundTracingActiveScenario::BackgroundTracingActiveScenario(
    std::unique_ptr<BackgroundTracingConfigImpl> config,
    TracingDelegate* delegate,
    base::OnceClosure on_aborted_callback)
    : config_(std::move(config)),
      delegate_(delegate),
      on_aborted_callback_(std::move(on_aborted_callback)) {
  DCHECK(config_ && !config_->rules().empty());
  for (const auto& rule : config_->rules()) {
    rule->Install(
        base::BindRepeating(&BackgroundTracingActiveScenario::OnRuleTriggered,
                            base::Unretained(this)));
  }
}

BackgroundTracingActiveScenario::~BackgroundTracingActiveScenario() {
  for (const auto& rule : config_->rules()) {
    rule->Uninstall();
  }
}

const BackgroundTracingConfigImpl* BackgroundTracingActiveScenario::GetConfig()
    const {
  return config_.get();
}

void BackgroundTracingActiveScenario::SetState(State new_state) {
  auto old_state = scenario_state_;
  scenario_state_ = new_state;

  if ((old_state == State::kTracing) &&
      base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
    // Leaving the kTracing state means we're supposed to have fully
    // shut down tracing at this point. Since StartTracing directly enables
    // tracing in TraceLog, in addition to going through Mojo, there's an
    // edge-case where tracing is rapidly stopped after starting, too quickly
    // for the TraceEventAgent of the browser process to register itself,
    // which means that we're left in a state where the Mojo interface doesn't
    // think we're tracing but TraceLog is still enabled. If that's the case,
    // we abort tracing here.
    DCHECK_NE(config_->tracing_mode(), BackgroundTracingConfigImpl::SYSTEM);
    base::trace_event::TraceLog::GetInstance()->SetDisabled(
        base::trace_event::TraceLog::RECORDING_MODE);
  }

  if (scenario_state_ == State::kAborted) {
    DCHECK_NE(config_->tracing_mode(), BackgroundTracingConfigImpl::SYSTEM);
    tracing_session_.reset();
    tracing_timer_.reset();

    std::move(on_aborted_callback_).Run();
  }
}

void BackgroundTracingActiveScenario::FireTimerForTesting() {
  DCHECK(tracing_timer_);
  tracing_timer_->FireTimerForTesting();
}

void BackgroundTracingActiveScenario::SetRuleTriggeredCallbackForTesting(
    const base::RepeatingClosure& callback) {
  rule_triggered_callback_for_testing_ = callback;
}

base::WeakPtr<BackgroundTracingActiveScenario>
BackgroundTracingActiveScenario::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BackgroundTracingActiveScenario::StartTracingIfConfigNeedsIt() {
  DCHECK(config_);
  if (config_->tracing_mode() == BackgroundTracingConfigImpl::PREEMPTIVE) {
    StartTracing();
  }

  // There is nothing to do in case of reactive tracing.
}

bool BackgroundTracingActiveScenario::StartTracing() {
  DCHECK_NE(config_->tracing_mode(), BackgroundTracingConfigImpl::SYSTEM);
  TraceConfig chrome_config = config_->GetTraceConfig();


  // Activate the categories immediately. StartTracing eventually does this
  // itself, but asynchronously via Mojo, and in the meantime events will be
  // dropped. This ensures that we start recording events for those categories
  // immediately.
  DCHECK(!tracing_session_);
  tracing_session_ =
      std::make_unique<TracingSession>(this, chrome_config, config_.get());

  SetState(State::kTracing);
  BackgroundTracingManagerImpl::RecordMetric(Metrics::RECORDING_ENABLED);
  return true;
}

void BackgroundTracingActiveScenario::BeginFinalizing() {
  DCHECK_NE(config_->tracing_mode(), BackgroundTracingConfigImpl::SYSTEM);
  tracing_timer_.reset();

  base::OnceClosure on_begin_finalization_success = base::BindOnce(
      [](base::WeakPtr<BackgroundTracingActiveScenario> weak_this) {
        if (!weak_this) {
          return;
        }

        weak_this->SetState(State::kFinalizing);
        BackgroundTracingManagerImpl::RecordMetric(
            Metrics::FINALIZATION_ALLOWED);
      },
      weak_ptr_factory_.GetWeakPtr());

  base::OnceClosure on_begin_finalization_failure = base::BindOnce(
      [](base::WeakPtr<BackgroundTracingActiveScenario> weak_this) {
        if (!weak_this) {
          return;
        }

        BackgroundTracingManagerImpl::RecordMetric(
            Metrics::FINALIZATION_DISALLOWED);
        weak_this->SetState(State::kAborted);
      },
      weak_ptr_factory_.GetWeakPtr());

  tracing_session_->BeginFinalizing(std::move(on_begin_finalization_success),
                                    std::move(on_begin_finalization_failure),
                                    last_triggered_rule_->is_crash());
}

void BackgroundTracingActiveScenario::OnProtoDataComplete(
    std::string&& serialized_trace) {
  BackgroundTracingManagerImpl::GetInstance().OnProtoDataComplete(
      std::move(serialized_trace), config_->scenario_name(),
      last_triggered_rule_->rule_id(), config_->requires_anonymized_data(),
      last_triggered_rule_->is_crash(), base::Token::CreateRandom());
  tracing_session_.reset();
  SetState(State::kIdle);

  // Now that a trace has completed, we may need to enable recording again.
  StartTracingIfConfigNeedsIt();
}

void BackgroundTracingActiveScenario::AbortScenario() {
  for (const auto& rule : config_->rules()) {
    rule->Uninstall();
  }
  if (tracing_session_) {
    tracing_session_->AbortScenario(base::BindRepeating(
        [](base::WeakPtr<BackgroundTracingActiveScenario> weak_this) {
          if (weak_this) {
            weak_this->SetState(State::kAborted);
          }
        },
        weak_ptr_factory_.GetWeakPtr()));
  } else if (config_->tracing_mode() == BackgroundTracingConfig::SYSTEM) {
    // We can't 'abort' system tracing since we aren't the consumer. Instead we
    // send a trigger into the system tracing so that we can tell the time the
    // scenario stopped.
    perfetto::Tracing::ActivateTriggers(
        {"org.chromium.background_tracing.scenario_aborted"}, /*ttl_ms=*/0);
  } else {
    // Setting the kAborted state will cause |this| to be destroyed.
    SetState(State::kAborted);
  }
}

bool BackgroundTracingActiveScenario::OnRuleTriggered(
    const BackgroundTracingRule* triggered_rule) {
  DCHECK_NE(state(), State::kAborted);

  last_triggered_rule_ = triggered_rule;

  base::TimeDelta trace_delay = triggered_rule->GetTraceDelay();

  switch (config_->tracing_mode()) {
    case BackgroundTracingConfigImpl::REACTIVE:
      // In reactive mode, a trigger starts tracing, or finalizes tracing
      // immediately if it's already running.
      BackgroundTracingManagerImpl::RecordMetric(Metrics::REACTIVE_TRIGGERED);

      if (state() != State::kTracing) {
        // It was not already tracing, start a new trace.
        if (!StartTracing()) {
          return false;
        }
      } else {
        return false;
      }
      break;
    case BackgroundTracingConfigImpl::SYSTEM:
      BackgroundTracingManagerImpl::RecordMetric(Metrics::SYSTEM_TRIGGERED);
      perfetto::Tracing::ActivateTriggers({triggered_rule->rule_id()},
                                          /*ttl_ms=*/0);
      if (!rule_triggered_callback_for_testing_.is_null()) {
        rule_triggered_callback_for_testing_.Run();
      }
      return true;
    case BackgroundTracingConfigImpl::PREEMPTIVE:
      // In preemptive mode, a trigger starts finalizing a trace if one is
      // running and we haven't got a finalization timer running,
      // otherwise we do nothing.
      if ((state() != State::kTracing) || tracing_timer_) {
        return false;
      }

      BackgroundTracingManagerImpl::RecordMetric(Metrics::PREEMPTIVE_TRIGGERED);
      break;
  }

  if (trace_delay.is_zero()) {
    BeginFinalizing();
  } else {
    tracing_timer_ = std::make_unique<TracingTimer>(this);
    tracing_timer_->StartTimer(trace_delay);
  }

  if (!rule_triggered_callback_for_testing_.is_null()) {
    rule_triggered_callback_for_testing_.Run();
  }
  return true;
}

void BackgroundTracingActiveScenario::GenerateMetadataProto(
    perfetto::protos::pbzero::ChromeMetadataPacket* metadata) {
  if (!last_triggered_rule_) {
    return;
  }

  auto* background_tracing_metadata =
      metadata->set_background_tracing_metadata();

  uint32_t scenario_name_hash = variations::HashName(config_->scenario_name());
  background_tracing_metadata->set_scenario_name_hash(scenario_name_hash);

  auto* triggered_rule = background_tracing_metadata->set_triggered_rule();
  last_triggered_rule_->GenerateMetadataProto(triggered_rule);
}

}  // namespace content
