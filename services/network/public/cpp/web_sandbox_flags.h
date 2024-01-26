// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_WEB_SANDBOX_FLAGS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_WEB_SANDBOX_FLAGS_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/component_export.h"

namespace network {
namespace mojom {

enum class WebSandboxFlags : int32_t;

inline constexpr WebSandboxFlags operator&(WebSandboxFlags a,
                                           WebSandboxFlags b) {
  return static_cast<WebSandboxFlags>(static_cast<int>(a) &
                                      static_cast<int>(b));
}

inline constexpr WebSandboxFlags operator|(WebSandboxFlags a,
                                           WebSandboxFlags b) {
  return static_cast<WebSandboxFlags>(static_cast<int>(a) |
                                      static_cast<int>(b));
}

inline WebSandboxFlags& operator|=(WebSandboxFlags& a, WebSandboxFlags b) {
  return a = a | b;
}

inline WebSandboxFlags& operator&=(WebSandboxFlags& a, WebSandboxFlags b) {
  return a = a & b;
}

inline constexpr WebSandboxFlags operator~(WebSandboxFlags flags) {
  return static_cast<WebSandboxFlags>(~static_cast<int>(flags));
}

}  // namespace mojom

// The output of |ParseSandboxPolicy(input)|.
struct WebSandboxFlagsParsingResult {
  // The parsed WebSandboxFlags policy.
  mojom::WebSandboxFlags flags;

  // The console error message to be displayed for invalid input. Empty when
  // there are no errors.
  std::string error_message;
};

// Parses a WebSandboxPolicy. The input is an unordered set of unique
// space-separated sandbox tokens.
// See: http://www.w3.org/TR/html5/the-iframe-element.html#attr-iframe-sandbox
//
// |ignored_flags| is used by experimental features to ignore some tokens when
// the corresponding feature is off.
//
// Supposed to be called only from a (semi-)sandboxed processes, i.e. from blink
// or from the network process. See: docs/security/rule-of-2.md.
COMPONENT_EXPORT(NETWORK_CPP)
WebSandboxFlagsParsingResult ParseWebSandboxPolicy(
    std::string_view input,
    mojom::WebSandboxFlags ignored_flags);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_WEB_SANDBOX_FLAGS_H_
