// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_tag.h"

#include <tuple>

#include "base/logging.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif  // OS_ANDROID

namespace net {

#if defined(OS_ANDROID)
// Expose UNSET_UID to Java.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
enum TrafficStatsUid {
  UNSET = -1,
};
// Java generator needs explicit integer, verify equality here.
static_assert(UNSET == SocketTag::UNSET_UID,
              "TrafficStatsUid does not match SocketTag::UNSET_UID");
#endif  // OS_ANDROID

bool SocketTag::operator<(const SocketTag& other) const {
#if defined(OS_ANDROID)
  return std::tie(uid_, traffic_stats_tag_) <
         std::tie(other.uid_, other.traffic_stats_tag_);
#else
  return false;
#endif  // OS_ANDROID
}

bool SocketTag::operator==(const SocketTag& other) const {
#if defined(OS_ANDROID)
  return std::tie(uid_, traffic_stats_tag_) ==
         std::tie(other.uid_, other.traffic_stats_tag_);
#else
  return true;
#endif  // OS_ANDROID
}

void SocketTag::Apply(SocketDescriptor socket) const {
#if defined(OS_ANDROID)
  net::android::TagSocket(socket, uid_, traffic_stats_tag_);
#else
  CHECK(false);
#endif  // OS_ANDROID
}

}  // namespace net
