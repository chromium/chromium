// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PERFETTO_TRACE_CONFIG_H_
#define REMOTING_BASE_PERFETTO_TRACE_CONFIG_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace remoting {

inline constexpr uint32_t kDefaultPerfettoBufferSizeKb = 16 * 1024;  // 16 MiB
inline constexpr uint32_t kMaxPerfettoBufferSizeKb = 64 * 1024;      // 64 MiB
inline constexpr uint32_t kDefaultPerfettoClearPeriodMs = 500;

struct PerfettoTraceConfig {
  PerfettoTraceConfig();
  PerfettoTraceConfig(const PerfettoTraceConfig&);
  PerfettoTraceConfig& operator=(const PerfettoTraceConfig&);
  PerfettoTraceConfig(PerfettoTraceConfig&&);
  PerfettoTraceConfig& operator=(PerfettoTraceConfig&&);
  ~PerfettoTraceConfig();

  // Enabled Perfetto TrackEvent categories or patterns (for example,
  // `"remoting.*"`, `"remoting.video"`, `"webrtc"`). Entries prefixed with
  // `'-'` (such as `"-remoting.debug"`) are added to `disabled_categories`.
  // All unlisted categories (except `"__metadata"`) are disabled via
  // `disabled_categories("*")`.
  std::vector<std::string> enabled_categories;

  // Recording duration. Zero (`base::TimeDelta()`) runs continuously until
  // explicitly stopped or cancelled.
  base::TimeDelta duration;

  // Per-process in-memory buffer size in KiB (`1 <= size <= 64 MiB`).
  uint32_t max_buffer_size_kb = kDefaultPerfettoBufferSizeKb;

  // When true (default for production Me2Me and flight-recorder sessions), uses
  // `FillPolicy::RING_BUFFER`. When false (optional for offline CLI
  // benchmarks), uses `FillPolicy::DISCARD`.
  bool ring_buffer = true;

  // Incremental state clear period in milliseconds. Periodic clearing ensures
  // ring-buffer wrap-around and post-hoc lookback trimming retain valid
  // `InternedData` and `TrackDescriptor` tables. Zero (`0`) disables periodic
  // incremental state clearing (leaving `incremental_state_config` unset).
  uint32_t clear_period_ms = kDefaultPerfettoClearPeriodMs;

  // Returns true if buffer bounds, duration, and categories are valid for
  // starting a trace session.
  bool IsValid() const;

  // Builds the underlying `perfetto::TraceConfig` proto. Note that `duration`
  // is intentionally not serialized into `perfetto::TraceConfig::duration_ms`
  // and is instead enforced by the trace producer/caller (so `TrackEvent`
  // buffers are explicitly flushed before stopping).
  perfetto::TraceConfig ToPerfettoConfig() const;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PERFETTO_TRACE_CONFIG_H_
