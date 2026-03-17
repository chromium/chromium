// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_platform_attempt_factory_android.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/dns/dns_attempt.h"
#include "net/dns/dns_platform_android_attempt.h"
#include "net/log/net_log_with_source.h"

namespace net {

std::unique_ptr<DnsPlatformAttemptFactoryAndroid>
DnsPlatformAttemptFactoryAndroid::Create() {
  base::NoDestructor<DnsPlatformAndroidAttempt::DelegateImpl> delegate;
  return base::WrapUnique<DnsPlatformAttemptFactoryAndroid>(
      new DnsPlatformAttemptFactoryAndroid(delegate.get()));
}

std::unique_ptr<DnsPlatformAttemptFactoryAndroid>
DnsPlatformAttemptFactoryAndroid::CreateForTesting(
    DnsPlatformAndroidAttempt::Delegate* delegate) {
  return base::WrapUnique<DnsPlatformAttemptFactoryAndroid>(
      new DnsPlatformAttemptFactoryAndroid(delegate));
}

DnsPlatformAttemptFactoryAndroid::DnsPlatformAttemptFactoryAndroid(
    DnsPlatformAndroidAttempt::Delegate* delegate)
    : delegate_(delegate) {}

std::unique_ptr<DnsAttempt>
DnsPlatformAttemptFactoryAndroid::CreateDnsPlatformAttempt(
    size_t server_index,
    base::span<const uint8_t> hostname,
    uint16_t dns_query_type,
    handles::NetworkHandle target_network,
    const NetLogWithSource& parent_net_log) {
  return std::make_unique<DnsPlatformAndroidAttempt>(
      server_index, hostname, dns_query_type, target_network, delegate_,
      parent_net_log);
}

DnsPlatformAttemptFactoryAndroid::~DnsPlatformAttemptFactoryAndroid() = default;

}  // namespace net
