// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_SECURE_DNS_POLICY_H_
#define NET_DNS_PUBLIC_SECURE_DNS_POLICY_H_

namespace net {

// The SecureDnsPolicy indicates whether and how a specific request or socket
// can use Secure DNS.
enum class SecureDnsPolicy {
  // Secure DNS is allowed for this request, if it is generally enabled.
  kAllow,
  // This request must not use Secure DNS, even when it is otherwise enabled.
  kDisable,
  // This request is part of the Secure DNS bootstrap process.
  kBootstrap,
};

const char* SecureDnsPolicyToDebugString(SecureDnsPolicy secure_dns_policy);

}  // namespace net

#endif  // NET_DNS_PUBLIC_SECURE_DNS_POLICY_H_
