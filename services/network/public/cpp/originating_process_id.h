// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_ID_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_ID_H_

#include <compare>

#include "base/component_export.h"
#include "services/network/public/cpp/renderer_process_id.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace network {

// This class describes a process that a network context is owned by.  This can
// either be the browser process or a specific untrusted renderer process.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) OriginatingProcessId {
 public:
  // Creates an invalid renderer process.
  OriginatingProcessId();

  OriginatingProcessId(const OriginatingProcessId&) = default;
  OriginatingProcessId& operator=(const OriginatingProcessId&) = default;

  // Create a new |OriginatingProcessId| that represents the browser process.
  static OriginatingProcessId browser();

  // Create a new |OriginatingProcessId| that represents a child renderer
  // process.
  static OriginatingProcessId renderer(RendererProcessId&& process_id);

  // Whether the originating process is the browser process.
  bool is_browser() const;

  // Get the renderer process ID for this, it is a bug to call this if
  // |is_browser| returns true.
  const RendererProcessId& renderer_process_id() const;

  // Returns true if this is an invalid renderer process.
  bool is_null() const;

  // TODO(crbug.com/379869738) Remove GetUnsafeValue.
  int32_t GetUnsafeValue() const;

  // TODO(crbug.com/379869738) Remove GetUnsafeValue.
  static OriginatingProcessId FromUnsafeValue(int32_t process_id);

  explicit operator bool() const { return !is_null(); }

  friend auto operator<=>(const OriginatingProcessId&,
                          const OriginatingProcessId&) = default;
  friend bool operator==(const OriginatingProcessId&,
                         const OriginatingProcessId&) = default;

 private:
  explicit OriginatingProcessId(
      std::optional<RendererProcessId>&& maybe_process_id);

  // If std::nullopt then the originating process is the browser process,
  // otherwise it references the renderer process.
  std::optional<RendererProcessId> process_id_;
};

COMPONENT_EXPORT(NETWORK_CPP_BASE)
void WriteIntoTracedValue(perfetto::TracedValue context,
                          const OriginatingProcessId& process);

}  // namespace network

template <>
struct COMPONENT_EXPORT(
    NETWORK_CPP_BASE) std::hash<network::OriginatingProcessId> {
  size_t operator()(const network::OriginatingProcessId& process) const;
};

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_ID_H_
