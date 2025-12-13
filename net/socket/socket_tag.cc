// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_tag.h"

#include <iostream>
#include <tuple>

#include "base/notreached.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_library.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace net {

#if BUILDFLAG(IS_ANDROID)
// Expose UNSET_UID to Java.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
enum TrafficStatsUid {
  UNSET_UID = -1,
};
// Java generator needs explicit integer, verify equality here.
static_assert(UNSET_UID == SocketTag::UNSET_UID,
              "TrafficStatsUid does not match SocketTag::UNSET_UID");
// Expose UNSET_TAG to Java.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
enum TrafficStatsTag {
  UNSET_TAG = -1,
};
static_assert(UNSET_TAG == SocketTag::UNSET_TAG,
              "TrafficStatsTag does not match SocketTag::UNSET_TAG");
#endif  // BUILDFLAG(IS_ANDROID)

bool SocketTag::operator<(const SocketTag& other) const {
#if BUILDFLAG(IS_ANDROID)
  return std::tie(uid_, traffic_stats_tag_) <
         std::tie(other.uid_, other.traffic_stats_tag_);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool SocketTag::operator==(const SocketTag& other) const {
#if BUILDFLAG(IS_ANDROID)
  return std::tie(uid_, traffic_stats_tag_) ==
         std::tie(other.uid_, other.traffic_stats_tag_);
#else
  return true;
#endif  // BUILDFLAG(IS_ANDROID)
}

void SocketTag::Apply(SocketDescriptor socket) const {
#if BUILDFLAG(IS_ANDROID)
  net::android::TagSocket(socket, uid_, traffic_stats_tag_);
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_ANDROID)
}

std::ostream& operator<<(std::ostream& os, const SocketTag& tag) {
#if BUILDFLAG(IS_ANDROID)
  os << "uid: " << tag.uid() << ", tag: " << tag.traffic_stats_tag();
#else
  os << "SocketTag()";
#endif  // BUILDFLAG(IS_ANDROID)
  return os;
}

}  // namespace net
