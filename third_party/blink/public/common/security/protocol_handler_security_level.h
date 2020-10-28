// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SECURITY_PROTOCOL_HANDLER_SECURITY_LEVEL_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SECURITY_PROTOCOL_HANDLER_SECURITY_LEVEL_H_
namespace blink {

// Levels of security used for Navigator's (un)registerProtocolHandler.
// This is an increasing hierarchy, starting from the default HTML5 behavior and
// for which each higher level removes a security restriction.
enum class ProtocolHandlerSecurityLevel {
  // Default behavior per the HTML5 specification.
  // https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
  kStrict,

  // Allow registration calls to cross-origin URLs.
  kUntrustedOrigins
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SECURITY_PROTOCOL_HANDLER_SECURITY_LEVEL_H_
