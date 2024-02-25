// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "components/tracing/common/tracing_switches.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"

namespace tracing {

// static
constexpr size_t PerfettoProducer::kSMBPageSizeBytes;

// static
constexpr size_t PerfettoProducer::kDefaultSMBSizeBytes;

PerfettoProducer::PerfettoProducer(
    base::tracing::PerfettoTaskRunner* task_runner)
    : task_runner_(task_runner) {
  DCHECK(task_runner_);
}

PerfettoProducer::~PerfettoProducer() = default;

bool PerfettoProducer::SetupStartupTracing(
    const base::trace_event::TraceConfig& trace_config,
    bool privacy_filtering_enabled) {
  // Abort if we were already startup tracing.
  if (startup_tracing_active_.exchange(true)) {
    return false;
  }

  if (!SetupSharedMemoryForStartupTracing()) {
    return false;
  }

  // Tell data sources to enable startup tracing, too.
  for (PerfettoTracedProcess::DataSourceBase* ds :
       PerfettoTracedProcess::Get()->data_sources()) {
    ds->SetupStartupTracing(this, trace_config, privacy_filtering_enabled);
  }

  MaybeScheduleStartupTracingTimeout();
  return true;
}

void PerfettoProducer::OnThreadPoolAvailable() {
  MaybeScheduleStartupTracingTimeout();
}

void PerfettoProducer::MaybeScheduleStartupTracingTimeout() {
  // We can't schedule the timeout until the thread pool is available. Note that
  // this method has a benign race in that it's possible that
  // SetupStartupTracing() is called concurrently to the creation of the task
  // runner and OnThreadPoolAvailable(). However, the worst that could happen
  // (assuming calling HasTaskRunner() is thread-safe), is that we could post
  // the timeout task below twice, the second one of which will become a no-op.
  if (!IsStartupTracingActive() ||
      !PerfettoTracedProcess::GetTaskRunner()->HasTaskRunner()) {
    return;
  }

  PerfettoTracedProcess::GetTaskRunner()
      ->GetOrCreateTaskRunner()
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PerfettoProducer::OnStartupTracingTimeout,
                         weak_ptr_factory_.GetWeakPtr()),
          startup_tracing_timeout_);
}

void PerfettoProducer::OnStartupTracingTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsStartupTracingActive()) {
    return;
  }

  LOG(WARNING) << "Startup tracing timed out (tracing service didn't start the "
                  "session?).";

  for (PerfettoTracedProcess::DataSourceBase* ds :
       PerfettoTracedProcess::Get()->data_sources()) {
    ds->AbortStartupTracing();
  }

  OnStartupTracingComplete();
}

void PerfettoProducer::OnStartupTracingComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  startup_tracing_active_.store(false);
}

bool PerfettoProducer::IsStartupTracingActive() {
  return startup_tracing_active_.load();
}

std::unique_ptr<perfetto::TraceWriter>
PerfettoProducer::CreateStartupTraceWriter(
    uint16_t target_buffer_reservation_id) {
  DCHECK(MaybeSharedMemoryArbiter());
  return MaybeSharedMemoryArbiter()->CreateStartupTraceWriter(
      target_buffer_reservation_id);
}

void PerfettoProducer::BindStartupTargetBuffer(
    uint16_t target_buffer_reservation_id,
    perfetto::BufferID startup_target_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(MaybeSharedMemoryArbiter());
  MaybeSharedMemoryArbiter()->BindStartupTargetBuffer(
      target_buffer_reservation_id, startup_target_buffer);
}

void PerfettoProducer::AbortStartupTracingForReservation(
    uint16_t target_buffer_reservation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(MaybeSharedMemoryArbiter());
  MaybeSharedMemoryArbiter()->AbortStartupTracingForReservation(
      target_buffer_reservation_id);
}

std::unique_ptr<perfetto::TraceWriter> PerfettoProducer::CreateTraceWriter(
    perfetto::BufferID target_buffer,
    perfetto::BufferExhaustedPolicy buffer_exhausted_policy) {
  DCHECK(MaybeSharedMemoryArbiter());
  // Chromium uses BufferExhaustedPolicy::kDrop to avoid stalling trace writers
  // when the chunks in the SMB are exhausted. Stalling could otherwise lead to
  // deadlocks in chromium, because a stalled mojo IPC thread could prevent
  // CommitRequest messages from reaching the perfetto service.
  buffer_exhausted_policy = perfetto::BufferExhaustedPolicy::kDrop;
  return MaybeSharedMemoryArbiter()->CreateTraceWriter(target_buffer,
                                                       buffer_exhausted_policy);
}

void PerfettoProducer::DeleteSoonForTesting(
    std::unique_ptr<PerfettoProducer> perfetto_producer) {
  PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner()->DeleteSoon(
      FROM_HERE, std::move(perfetto_producer));
}

void PerfettoProducer::ResetSequenceForTesting() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

base::tracing::PerfettoTaskRunner* PerfettoProducer::task_runner() {
  return task_runner_;
}

size_t PerfettoProducer::GetPreferredSmbSizeBytes() {
  std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTraceSmbSize);
  if (switch_value.empty())
    return kDefaultSMBSizeBytes;
  uint64_t switch_kilobytes;
  if (!base::StringToUint64(switch_value, &switch_kilobytes) ||
      (switch_kilobytes * 1024) % kSMBPageSizeBytes != 0) {
    LOG(WARNING) << "Invalid tracing SMB size: " << switch_value;
    return kDefaultSMBSizeBytes;
  }
  return switch_kilobytes * 1024;
}

}  // namespace tracing
