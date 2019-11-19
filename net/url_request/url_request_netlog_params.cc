// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_netlog_params.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/log/net_log_capture_mode.h"
#include "url/gurl.h"

namespace net {

base::Value NetLogURLRequestConstructorParams(
    const GURL& url,
    RequestPriority priority,
    NetworkTrafficAnnotationTag traffic_annotation) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("url", url.possibly_invalid_spec());
  dict.SetStringKey("priority", RequestPriorityToString(priority));
  dict.SetIntKey("traffic_annotation", traffic_annotation.unique_id_hash_code);
  return dict;
}

base::Value NetLogURLRequestStartParams(const GURL& url,
                                        const std::string& method,
                                        int load_flags,
                                        PrivacyMode privacy_mode,
                                        int64_t upload_id) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("url", url.possibly_invalid_spec());
  dict.SetStringKey("method", method);
  dict.SetIntKey("load_flags", load_flags);
  dict.SetIntKey("privacy_mode", privacy_mode == PRIVACY_MODE_ENABLED);
  if (upload_id > -1)
    dict.SetStringKey("upload_id", base::NumberToString(upload_id));
  return dict;
}

}  // namespace net
