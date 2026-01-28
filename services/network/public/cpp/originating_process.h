// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_H_

#include <compare>

#include "base/component_export.h"
#include "services/network/public/cpp/renderer_process.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace network {

// This class describes a process that a network context is owned by.  This can
// either be the browser process or a specific untrusted renderer process.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) OriginatingProcess {
 public:
  // Creates an invalid renderer process.
  OriginatingProcess();

  OriginatingProcess(const OriginatingProcess&) = default;
  OriginatingProcess& operator=(const OriginatingProcess&) = default;

  // Create a new |OriginatingProcess| that represents the browser process.
  static OriginatingProcess browser();

  // Create a new |OriginatingProcess| that represents a child renderer process.
  static OriginatingProcess renderer(RendererProcess&& process_id);

  // Whether the originating process is the browser process.
  bool is_browser() const;

  // Get the renderer process ID for this, it is a bug to call this if
  // |is_browser| returns true.
  const RendererProcess& renderer_process() const;

  // Returns true if this is an invalid renderer process.
  bool is_null() const;

  // TODO(crbug.com/379869738) Remove GetUnsafeValue.
  int32_t GetUnsafeValue() const;

  explicit operator bool() const { return !is_null(); }

  friend auto operator<=>(const OriginatingProcess&,
                          const OriginatingProcess&) = default;
  friend bool operator==(const OriginatingProcess&,
                         const OriginatingProcess&) = default;

 private:
  explicit OriginatingProcess(
      std::optional<RendererProcess>&& maybe_process_id);

  // If std::nullopt then the originating process is the browser process,
  // otherwise it references the renderer process.
  std::optional<RendererProcess> process_id_;
};

COMPONENT_EXPORT(NETWORK_CPP_BASE)
void WriteIntoTracedValue(perfetto::TracedValue context,
                          const OriginatingProcess& process);

}  // namespace network

template <>
struct COMPONENT_EXPORT(
    NETWORK_CPP_BASE) std::hash<network::OriginatingProcess> {
  size_t operator()(const network::OriginatingProcess& process) const;
};

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ORIGINATING_PROCESS_H_
