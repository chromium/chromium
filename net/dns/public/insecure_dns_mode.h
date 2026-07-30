// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_INSECURE_DNS_MODE_H_
#define NET_DNS_PUBLIC_INSECURE_DNS_MODE_H_

namespace net {

// Defines the mode of operation of the insecure portion of the built-in
// DNS resolver.
enum class InsecureDnsMode {
  // Insecure DNS is disabled.
  kDisabled,
  // Insecure DNS is enabled using the built-in DNS client.
  kEnabledBuiltIn,
  // Insecure DNS is enabled using the platform DNS APIs.
  kEnabledPlatform,
  // Insecure DNS is enabled using platform DNS APIs, replacing
  // TaskType::SYSTEM,
  // with TaskType::DNS disabled and no fallback to TaskType::SYSTEM.
  kEnabledPlatformNoSystem,
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_INSECURE_DNS_MODE_H_
