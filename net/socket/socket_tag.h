// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_TAG_H_
#define NET_SOCKET_SOCKET_TAG_H_

#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/socket/socket_descriptor.h"

#if BUILDFLAG(IS_ANDROID)
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace net {

// SocketTag represents a tag that can be applied to a socket. Currently only
// implemented for Android, it facilitates assigning a Android TrafficStats tag
// and UID to a socket so that future network data usage by the socket is
// attributed to the tag and UID that the socket is tagged with.
//
// This class is small (<=64-bits) and contains only POD to facilitate default
// copy and assignment operators so that it can easily be passed by value.
class NET_EXPORT SocketTag {
 public:
#if BUILDFLAG(IS_ANDROID)
  // Default constructor doesn't set any tags.
  SocketTag() : SocketTag(UNSET_UID, UNSET_TAG) {}
  // Create a SocketTag with given UID |uid| and |traffic_stats_tag|.
  SocketTag(uid_t uid, int32_t traffic_stats_tag)
      : uid_(uid), traffic_stats_tag_(traffic_stats_tag) {}
#else
  SocketTag() = default;
#endif  // BUILDFLAG(IS_ANDROID)
  ~SocketTag() = default;

  bool operator<(const SocketTag& other) const;
  bool operator==(const SocketTag& other) const;
  bool operator!=(const SocketTag& other) const { return !(*this == other); }

  // Apply this tag to |socket|.
  void Apply(SocketDescriptor socket) const;

#if BUILDFLAG(IS_ANDROID)
  // Values to indicate no UID or tag should be set. These values match those in
  // Android:
  // http://androidxref.com/4.4_r1/xref/frameworks/base/core/java/android/net/TrafficStats.java#147
  // http://androidxref.com/4.4_r1/xref/frameworks/base/core/java/android/net/TrafficStats.java#169
  static const uid_t UNSET_UID = -1;
  static const int32_t UNSET_TAG = -1;

 private:
  // UID to tag with.
  uid_t uid_;
  // TrafficStats tag to tag with.
  int32_t traffic_stats_tag_;
#endif  // BUILDFLAG(IS_ANDROID)
  // Copying and assignment are allowed.
};

}  // namespace net

#endif  // NET_SOCKET_SOCKET_TAG_H_
