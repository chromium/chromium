// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_netlog_params.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/site_for_cookies.h"
#include "net/log/net_log_capture_mode.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

base::Value::Dict NetLogURLRequestConstructorParams(
    const GURL& url,
    RequestPriority priority,
    NetworkTrafficAnnotationTag traffic_annotation) {
  base::Value::Dict dict;
  dict.Set("url", url.possibly_invalid_spec());
  dict.Set("priority", RequestPriorityToString(priority));
  dict.Set("traffic_annotation", traffic_annotation.unique_id_hash_code);
  return dict;
}

base::Value::Dict NetLogURLRequestStartParams(
    const GURL& url,
    const std::string& method,
    int load_flags,
    const IsolationInfo& isolation_info,
    const SiteForCookies& site_for_cookies,
    const std::optional<url::Origin>& initiator,
    int64_t upload_id) {
  base::Value::Dict dict;
  dict.Set("url", url.possibly_invalid_spec());
  dict.Set("method", method);
  dict.Set("load_flags", load_flags);
  dict.Set("network_isolation_key",
           isolation_info.network_isolation_key().ToDebugString());
  std::string request_type;
  switch (isolation_info.request_type()) {
    case IsolationInfo::RequestType::kMainFrame:
      request_type = "main frame";
      break;
    case IsolationInfo::RequestType::kSubFrame:
      request_type = "subframe";
      break;
    case IsolationInfo::RequestType::kOther:
      request_type = "other";
      break;
  }
  dict.Set("request_type", request_type);
  dict.Set("site_for_cookies", site_for_cookies.ToDebugString());
  dict.Set("initiator",
           initiator.has_value() ? initiator->Serialize() : "not an origin");
  if (upload_id > -1)
    dict.Set("upload_id", base::NumberToString(upload_id));
  return dict;
}

}  // namespace net
