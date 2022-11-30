// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_MDNS_LISTENER_UPDATE_TYPE_H_
#define NET_DNS_PUBLIC_MDNS_LISTENER_UPDATE_TYPE_H_

namespace net {

// Types of update notifications from a HostResolver::MdnsListener
enum class MdnsListenerUpdateType {
  kAdded,
  kChanged,
  kRemoved,
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_MDNS_LISTENER_UPDATE_TYPE_H_
