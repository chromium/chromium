// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MOCK_DNS_PLATFORM_ANDROID_ATTEMPT_H_
#define NET_DNS_MOCK_DNS_PLATFORM_ANDROID_ATTEMPT_H_

#include <android/multinetwork.h>
#include <android/versioning.h>
#include <arpa/nameser.h>
#include <stdint.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <memory>
#include <ranges>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/strings/cstring_view.h"
#include "base/test/test_future.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_platform_android_attempt.h"
#include "net/dns/public/dns_query_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class MockAndroidDnsPlatformAttemptDelegate
    : public DnsPlatformAndroidAttempt::Delegate {
 public:
  static base::ScopedFD CreateFdWithUnreadData();
  static base::ScopedFD CreateFdWithNoData();

  MockAndroidDnsPlatformAttemptDelegate();
  ~MockAndroidDnsPlatformAttemptDelegate() override;

  MOCK_METHOD(int,
              Query,
              (net_handle_t, base::cstring_view, uint16_t),
              (override));

  MOCK_METHOD(int, Result, (int, int*, base::span<uint8_t>), (override));
};

}  // namespace net

#endif  // NET_DNS_MOCK_DNS_PLATFORM_ANDROID_ATTEMPT_H_
