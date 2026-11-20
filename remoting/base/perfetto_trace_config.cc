// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/perfetto_trace_config.h"

#include <algorithm>
#include <string_view>

#include "base/strings/string_util.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_config.h"
#include "third_party/perfetto/protos/perfetto/common/builtin_clock.gen.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"

namespace remoting {

PerfettoTraceConfig::PerfettoTraceConfig() = default;
PerfettoTraceConfig::PerfettoTraceConfig(const PerfettoTraceConfig&) = default;
PerfettoTraceConfig& PerfettoTraceConfig::operator=(
    const PerfettoTraceConfig&) = default;
PerfettoTraceConfig::PerfettoTraceConfig(PerfettoTraceConfig&&) = default;
PerfettoTraceConfig& PerfettoTraceConfig::operator=(PerfettoTraceConfig&&) =
    default;
PerfettoTraceConfig::~PerfettoTraceConfig() = default;

bool PerfettoTraceConfig::IsValid() const {
  if (max_buffer_size_kb == 0 ||
      max_buffer_size_kb > kMaxPerfettoBufferSizeKb || duration.is_negative()) {
    return false;
  }

  // Require at least one non-empty, non-exclusion category entry.
  return std::ranges::any_of(
      enabled_categories, [](const std::string& raw_entry) {
        std::string_view entry =
            base::TrimWhitespaceASCII(raw_entry, base::TRIM_ALL);
        return !entry.empty() && !entry.starts_with('-');
      });
}

perfetto::TraceConfig PerfettoTraceConfig::ToPerfettoConfig() const {
  perfetto::TraceConfig perfetto_config;

  auto* buffer_config = perfetto_config.add_buffers();
  buffer_config->set_size_kb(max_buffer_size_kb);
  buffer_config->set_fill_policy(
      ring_buffer ? perfetto::TraceConfig::BufferConfig::RING_BUFFER
                  : perfetto::TraceConfig::BufferConfig::DISCARD);

  perfetto_config.mutable_builtin_data_sources()->set_primary_trace_clock(
      perfetto::protos::gen::BUILTIN_CLOCK_MONOTONIC);

  if (clear_period_ms > 0) {
    perfetto_config.mutable_incremental_state_config()->set_clear_period_ms(
        clear_period_ms);
  }

  perfetto::protos::gen::TrackEventConfig track_event_config;
  bool wildcard_enabled = false;
  for (const std::string& raw_entry : enabled_categories) {
    std::string_view entry =
        base::TrimWhitespaceASCII(raw_entry, base::TRIM_ALL);
    if (entry.empty()) {
      continue;
    }
    if (entry.starts_with('-')) {
      entry.remove_prefix(1);
      entry = base::TrimWhitespaceASCII(entry, base::TRIM_ALL);
      if (!entry.empty()) {
        track_event_config.add_disabled_categories(std::string(entry));
      }
      continue;
    }
    if (entry == "*") {
      wildcard_enabled = true;
    }
    track_event_config.add_enabled_categories(std::string(entry));
  }
  if (!wildcard_enabled) {
    track_event_config.add_disabled_categories("*");
  }
  track_event_config.add_enabled_categories("__metadata");
  track_event_config.set_enable_thread_time_sampling(true);

  auto* data_source_config =
      perfetto_config.add_data_sources()->mutable_config();
  data_source_config->set_name("track_event");
  data_source_config->set_target_buffer(0);
  data_source_config->set_track_event_config_raw(
      track_event_config.SerializeAsString());

  return perfetto_config;
}

}  // namespace remoting
