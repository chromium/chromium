// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_
#define NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"
#include "net/base/isolation_info.h"
#include "net/base/net_export.h"
#include "net/base/privacy_mode.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_capture_mode.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class GURL;

namespace url {
class Origin;
}

namespace net {

class SiteForCookies;

// Returns a Value containing NetLog parameters for constructing a URLRequest.
NET_EXPORT base::Value::Dict NetLogURLRequestConstructorParams(
    const GURL& url,
    RequestPriority priority,
    NetworkTrafficAnnotationTag traffic_annotation);

// Returns a Value containing NetLog parameters for starting a URLRequest.
NET_EXPORT base::Value::Dict NetLogURLRequestStartParams(
    const GURL& url,
    const std::string& method,
    int load_flags,
    const IsolationInfo& isolation_info,
    const SiteForCookies& site_for_cookies,
    const std::optional<url::Origin>& initiator,
    int64_t upload_id);

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_NETLOG_PARAMS_H_
