// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/etw_tracing_agent_win.h"

#include <stdint.h>

#include <utility>

#include "base/base64.h"
#include "base/json/json_string_value_serializer.h"
#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

EtwTracingAgent* g_etw_tracing_agent = nullptr;

const char kETWTraceLabel[] = "systemTraceEvents";

const int kEtwBufferSizeInKBytes = 16;
const int kEtwBufferFlushTimeoutInSeconds = 1;

std::string GuidToString(const GUID& guid) {
  return base::StringPrintf("%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                            guid.Data1, guid.Data2, guid.Data3, guid.Data4[0],
                            guid.Data4[1], guid.Data4[2], guid.Data4[3],
                            guid.Data4[4], guid.Data4[5], guid.Data4[6],
                            guid.Data4[7]);
}

}  // namespace

EtwTracingAgent::EtwTracingAgent(service_manager::Connector* connector)
    : BaseAgent(connector,
                kETWTraceLabel,
                tracing::mojom::TraceDataType::OBJECT,
                false /* supports_explicit_clock_sync */,
                base::kNullProcessId),
      thread_("EtwConsumerThread"),
      is_tracing_(false) {
  DCHECK(!g_etw_tracing_agent);
  g_etw_tracing_agent = this;
}

EtwTracingAgent::~EtwTracingAgent() {
  if (is_tracing_)
    StopKernelSessionTracing();
  g_etw_tracing_agent = nullptr;
}

void EtwTracingAgent::StartTracing(const std::string& config,
                                   base::TimeTicks coordinator_time,
                                   Agent::StartTracingCallback callback) {
  base::trace_event::TraceConfig trace_config(config);
  // Activate kernel tracing.
  if (!trace_config.IsSystraceEnabled() || !StartKernelSessionTracing()) {
    std::move(callback).Run(false /* success */);
    return;
  }
  is_tracing_ = true;

  // Start the consumer thread and start consuming events.
  thread_.Start();

  // Tracing agents, e.g. this, live as long as BrowserMainLoop lives and so
  // using base::Unretained here is safe.
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&EtwTracingAgent::TraceAndConsumeOnThread,
                                base::Unretained(this)));
  std::move(callback).Run(true /* success */);
}

void EtwTracingAgent::StopAndFlush(tracing::mojom::RecorderPtr recorder) {
  DCHECK(is_tracing_);
  // Deactivate kernel tracing.
  if (!StopKernelSessionTracing()) {
    LOG(FATAL) << "Could not stop system tracing.";
  }
  recorder_ = std::move(recorder);
  // Stop consuming and flush events. Tracing agents, e.g. this, live as long as
  // BrowserMainLoop lives and so using base::Unretained here is safe.
  thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&EtwTracingAgent::FlushOnThread, base::Unretained(this)));
}

void EtwTracingAgent::OnStopSystemTracingDone(const std::string& output) {
  recorder_->AddChunk(output);
  recorder_.reset();
  // Stop the consumer thread.
  thread_.Stop();
  is_tracing_ = false;
}

bool EtwTracingAgent::StartKernelSessionTracing() {
  // Enabled flags (tracing facilities).
  uint32_t enabled_flags = EVENT_TRACE_FLAG_IMAGE_LOAD |
                           EVENT_TRACE_FLAG_PROCESS | EVENT_TRACE_FLAG_THREAD |
                           EVENT_TRACE_FLAG_CSWITCH;

  EVENT_TRACE_PROPERTIES& p = *properties_.get();
  p.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
  p.FlushTimer = kEtwBufferFlushTimeoutInSeconds;
  p.BufferSize = kEtwBufferSizeInKBytes;
  p.LogFileNameOffset = 0;
  p.EnableFlags = enabled_flags;
  p.Wnode.ClientContext = 1;  // QPC timer accuracy.

  HRESULT hr = base::win::EtwTraceController::Start(
      KERNEL_LOGGER_NAME, &properties_, &session_handle_);

  // It's possible that a previous tracing session has been orphaned. If so
  // try stopping and restarting it.
  if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
    VLOG(1) << "Session already exists, stopping and restarting it.";
    hr = base::win::EtwTraceController::Stop(
        KERNEL_LOGGER_NAME, &properties_);
    if (FAILED(hr)) {
      VLOG(1) << "EtwTraceController::Stop failed with " << hr << ".";
      return false;
    }

    // The session was successfully shutdown so try to restart it.
    hr = base::win::EtwTraceController::Start(
        KERNEL_LOGGER_NAME, &properties_, &session_handle_);
  }

  if (FAILED(hr)) {
    VLOG(1) << "EtwTraceController::Start failed with " << hr << ".";
    return false;
  }

  return true;
}

bool EtwTracingAgent::StopKernelSessionTracing() {
  HRESULT hr = base::win::EtwTraceController::Stop(
      KERNEL_LOGGER_NAME, &properties_);
  return SUCCEEDED(hr);
}

// static
EtwTracingAgent* EtwTracingAgent::GetInstance() {
  return g_etw_tracing_agent;
}

// static
void EtwTracingAgent::ProcessEvent(EVENT_TRACE* event) {
  auto* instance = EtwTracingAgent::GetInstance();
  // Ignore events that are received after the browser is closed.
  if (instance)
    instance->AppendEventToBuffer(event);
}

void EtwTracingAgent::AddSyncEventToBuffer() {
  // Sync the clocks.
  base::Time walltime = base::subtle::TimeNowFromSystemTimeIgnoringOverride();
  base::TimeTicks now = TRACE_TIME_TICKS_NOW();

  LARGE_INTEGER walltime_in_us;
  walltime_in_us.QuadPart = walltime.ToInternalValue();
  LARGE_INTEGER now_in_us;
  now_in_us.QuadPart = now.ToInternalValue();

  // Add fields to the event.
  auto value = std::make_unique<base::DictionaryValue>();
  value->SetString("guid", "ClockSync");
  value->SetString("walltime",
                   base::StringPrintf("%08lX%08lX", walltime_in_us.HighPart,
                                      walltime_in_us.LowPart));
  value->SetString("tick", base::StringPrintf("%08lX%08lX", now_in_us.HighPart,
                                              now_in_us.LowPart));

  // Append it to the events buffer.
  events_->Append(std::move(value));
}

void EtwTracingAgent::AppendEventToBuffer(EVENT_TRACE* event) {
  auto value = std::make_unique<base::DictionaryValue>();

  // Add header fields to the event.
  LARGE_INTEGER ts_us;
  ts_us.QuadPart = event->Header.TimeStamp.QuadPart / 10;
  value->SetString(
      "ts", base::StringPrintf("%08lX%08lX", ts_us.HighPart, ts_us.LowPart));

  value->SetString("guid", GuidToString(event->Header.Guid));

  value->SetInteger("op", event->Header.Class.Type);
  value->SetInteger("ver", event->Header.Class.Version);
  value->SetInteger("pid", static_cast<int>(event->Header.ProcessId));
  value->SetInteger("tid", static_cast<int>(event->Header.ThreadId));
  value->SetInteger("cpu", event->BufferContext.ProcessorNumber);

  // Base64 encode the payload bytes.
  base::StringPiece buffer(static_cast<const char*>(event->MofData),
                           event->MofLength);
  std::string payload;
  base::Base64Encode(buffer, &payload);
  value->SetString("payload", payload);

  // Append it to the events buffer.
  events_->Append(std::move(value));
}

void EtwTracingAgent::TraceAndConsumeOnThread() {
  // Create the events buffer.
  events_.reset(new base::ListValue());

  // Output a clock sync event.
  AddSyncEventToBuffer();

  HRESULT hr = OpenRealtimeSession(KERNEL_LOGGER_NAME);
  if (FAILED(hr))
    return;
  Consume();
  Close();
}

void EtwTracingAgent::FlushOnThread() {
  // Add the header information to the stream.
  auto header = std::make_unique<base::DictionaryValue>();
  header->SetString("name", "ETW");

  // Release and pass the events buffer.
  header->Set("content", std::move(events_));

  // Serialize the results as a JSon string.
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.Serialize(*header.get());
  // TODO(chiniforooshan): Find a way to eliminate the extra string copy here.
  // This is not too bad for now, since it happens only once when tracing is
  // stopped.
  DCHECK_EQ('{', output.front());
  DCHECK_EQ('}', output.back());
  output = output.substr(1, output.size() - 2);

  // Tracing agents, e.g. this, live as long as BrowserMainLoop lives and so
  // using base::Unretained here is safe.
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::Bind(&EtwTracingAgent::OnStopSystemTracingDone,
                                      base::Unretained(this), output));
}

}  // namespace content
