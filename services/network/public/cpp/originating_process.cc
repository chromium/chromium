// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/originating_process.h"

#include "base/check.h"

namespace {
// This is the magic legacy value for the browser process, we keep using it
// internally for now during transition and to maintain hash consistency.
constexpr int32_t kBrowserProcess = 0;
}  // namespace

namespace network {

OriginatingProcess::OriginatingProcess() : process_id_(RendererProcess()) {}

OriginatingProcess::OriginatingProcess(
    std::optional<RendererProcess>&& maybe_process_id)
    : process_id_(std::move(maybe_process_id)) {
  DCHECK(!process_id_.has_value() || process_id_->value() != kBrowserProcess);
}

// static
OriginatingProcess OriginatingProcess::browser() {
  return OriginatingProcess(std::nullopt);
}

// static
OriginatingProcess OriginatingProcess::renderer(RendererProcess&& process_id) {
  return OriginatingProcess(std::move(process_id));
}

bool OriginatingProcess::is_browser() const {
  return !process_id_.has_value();
}

const RendererProcess& OriginatingProcess::renderer_process() const {
  CHECK(process_id_.has_value());
  return process_id_.value();
}

bool OriginatingProcess::is_null() const {
  return process_id_.has_value() && process_id_->is_null();
}

int32_t OriginatingProcess::GetUnsafeValue() const {
  return is_browser() ? kBrowserProcess : renderer_process().GetUnsafeValue();
}

void WriteIntoTracedValue(perfetto::TracedValue context,
                          const OriginatingProcess& process) {
  // Implement in the legacy form of browser processes having a value of 0.
  if (process.is_browser()) {
    std::move(context).WriteInt64(kBrowserProcess);
  } else {
    WriteIntoTracedValue(std::move(context), process.renderer_process());
  }
}

}  // namespace network

size_t std::hash<network::OriginatingProcess>::operator()(
    const network::OriginatingProcess& process) const {
  if (process.is_browser()) {
    return std::hash<int32_t>()(kBrowserProcess);
  }
  return std::hash<network::RendererProcess>()(process.renderer_process());
}
