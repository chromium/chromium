// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_PLATFORM_ATTEMPT_FACTORY_ANDROID_H_
#define NET_DNS_DNS_PLATFORM_ATTEMPT_FACTORY_ANDROID_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/dns/dns_attempt.h"
#include "net/dns/dns_platform_android_attempt.h"
#include "net/dns/dns_platform_attempt_factory.h"
#include "net/log/net_log_with_source.h"

namespace net {

// An implementation of DnsPlatformAttemptFactory specific to Android. It relies
// on DnsPlatformAndroidAttempt. For testing, it offers creating a variant that
// allows injecting a mock DnsPlatformAndroidAttempt::Delegate.
class NET_EXPORT DnsPlatformAttemptFactoryAndroid
    : public DnsPlatformAttemptFactory {
 public:
  static std::unique_ptr<DnsPlatformAttemptFactoryAndroid> Create();
  static std::unique_ptr<DnsPlatformAttemptFactoryAndroid> CreateForTesting(
      DnsPlatformAndroidAttempt::Delegate* delegate);

  ~DnsPlatformAttemptFactoryAndroid() override;

  // DnsPlatformAttemptFactory method:
  std::unique_ptr<DnsAttempt> CreateDnsPlatformAttempt(
      size_t server_index,
      base::span<const uint8_t> hostname,
      uint16_t dns_query_type,
      handles::NetworkHandle target_network,
      const NetLogWithSource& parent_net_log) override;

 private:
  explicit DnsPlatformAttemptFactoryAndroid(
      DnsPlatformAndroidAttempt::Delegate* delegate);

  // Must outlive `this`.
  raw_ptr<DnsPlatformAndroidAttempt::Delegate> delegate_;
};

}  // namespace net

#endif  // NET_DNS_DNS_PLATFORM_ATTEMPT_FACTORY_ANDROID_H_
