// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TEST_TRACING_SESSION_H_
#define CONTENT_BROWSER_TRACING_TEST_TRACING_SESSION_H_

#include <functional>

#include "base/token.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace content {

// Fake perfetto::TracingSession.
class TestTracingSession : public perfetto::TracingSession {
 public:
  static constexpr base::Token kClonedSessionId = base::Token(0xAB, 0xCD);

  TestTracingSession();
  ~TestTracingSession() override;

  void Setup(const perfetto::TraceConfig& config, int fd = -1) override;
  void Start() override;
  void StartBlocking() override;

  void SetOnStartCallback(std::function<void()> on_start) override;  // nocheck

  void SetOnErrorCallback(
      std::function<void(perfetto::TracingError)> on_error)  // nocheck
      override;

  void Flush(std::function<void(bool)>,  // nocheck
             uint32_t timeout_ms = 0) override;

  void Stop() override;

  void CloneTrace(CloneTraceArgs args,
                  CloneTraceCallback on_session_cloned) override;

  void StopBlocking() override;

  void SetOnStopCallback(std::function<void()> on_stop) override;  // nocheck

  void ChangeTraceConfig(const perfetto::TraceConfig&) override;
  void ReadTrace(ReadTraceCallback read_callback) override;
  void GetTraceStats(GetTraceStatsCallback) override;
  void QueryServiceState(QueryServiceStateCallback) override;

 private:
  std::function<void()> on_start_callback_;                        // nocheck
  std::function<void()> on_stop_callback_;                         // nocheck
  std::function<void(perfetto::TracingError)> on_error_callback_;  // nocheck
  bool start_should_fail_ = false;
  bool should_spuriously_stop = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TEST_TRACING_SESSION_H_
