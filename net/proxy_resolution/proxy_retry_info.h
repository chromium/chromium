// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RETRY_INFO_H_
#define NET_PROXY_RESOLUTION_PROXY_RETRY_INFO_H_

#include <map>

#include "base/time/time.h"

namespace net {

// Contains the information about when to retry a proxy server.
struct ProxyRetryInfo {
  ProxyRetryInfo() = default;

  // We should not retry until this time.
  base::TimeTicks bad_until;

  // This is the current delay. If the proxy is still bad, we need to increase
  // this delay.
  base::TimeDelta current_delay;

  // True if this proxy should be considered even if still bad.
  bool try_while_bad = true;

  // The network error received when this proxy failed, or |OK| if the proxy
  // was added to the retry list for a non-network related reason. (e.g. local
  // policy).
  int net_error = 0;
};

// Map of proxy servers with the associated RetryInfo structures.
// The key is a proxy URI string [<scheme>"://"]<host>":"<port>.
typedef std::map<std::string, ProxyRetryInfo> ProxyRetryInfoMap;

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RETRY_INFO_H_
