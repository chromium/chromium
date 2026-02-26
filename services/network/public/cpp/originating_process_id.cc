// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/originating_process_id.h"

#include "base/check.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"

namespace {
// This is the magic legacy value for the browser process, we keep using it
// internally for now during transition and to maintain hash consistency.
constexpr int32_t kBrowserProcessId = 0;
}  // namespace

namespace network {

OriginatingProcessId::OriginatingProcessId()
    : process_id_(RendererProcessId()) {}

OriginatingProcessId::OriginatingProcessId(
    std::optional<RendererProcessId>&& maybe_process_id)
    : process_id_(std::move(maybe_process_id)) {
  DCHECK(!process_id_.has_value() || process_id_->value() != kBrowserProcessId);
}

// static
OriginatingProcessId OriginatingProcessId::browser() {
  return OriginatingProcessId(std::nullopt);
}

// static
OriginatingProcessId OriginatingProcessId::renderer(
    RendererProcessId&& process_id) {
  return OriginatingProcessId(std::move(process_id));
}

bool OriginatingProcessId::is_browser() const {
  return !process_id_.has_value();
}

const RendererProcessId& OriginatingProcessId::renderer_process_id() const {
  CHECK(process_id_.has_value());
  return process_id_.value();
}

bool OriginatingProcessId::is_null() const {
  return process_id_.has_value() && process_id_->is_null();
}

int32_t OriginatingProcessId::GetUnsafeValue() const {
  return is_browser() ? kBrowserProcessId
                      : renderer_process_id().GetUnsafeValue();
}

// static
OriginatingProcessId OriginatingProcessId::FromUnsafeValue(int32_t process_id) {
  return process_id == kBrowserProcessId
             ? OriginatingProcessId::browser()
             : OriginatingProcessId::renderer(RendererProcessId(process_id));
}

void WriteIntoTracedValue(perfetto::TracedValue context,
                          const OriginatingProcessId& process) {
  // Implement in the legacy form of browser processes having a value of 0.
  if (process.is_browser()) {
    std::move(context).WriteInt64(kBrowserProcessId);
  } else {
    WriteIntoTracedValue(std::move(context), process.renderer_process_id());
  }
}

}  // namespace network

size_t std::hash<network::OriginatingProcessId>::operator()(
    const network::OriginatingProcessId& process) const {
  if (process.is_browser()) {
    return std::hash<int32_t>()(kBrowserProcessId);
  }
  return std::hash<network::RendererProcessId>()(process.renderer_process_id());
}
