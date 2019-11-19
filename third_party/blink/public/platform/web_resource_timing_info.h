// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RESOURCE_TIMING_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RESOURCE_TIMING_INFO_H_

#include <stdint.h>

#include "base/time/time.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_load_timing.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"

namespace blink {

// The browser-side equivalent to this struct is content::ServerTimingInfo.
// Note: Please update operator==() whenever a new field is added.
// TODO(dcheng): Migrate this struct over to Mojo so it doesn't need to be
// duplicated in //content and //third_party/blink.
struct WebServerTimingInfo {
  WebServerTimingInfo(const WebString& name,
                      double duration,
                      const WebString& description)
      : name(name), duration(duration), description(description) {}

#if INSIDE_BLINK
  bool operator==(const WebServerTimingInfo&) const;
  bool operator!=(const WebServerTimingInfo&) const;
#endif

  WebString name;
  double duration = 0.0;
  WebString description;
};

// This struct holds the information from PerformanceResourceTiming that needs
// to be passed between processes. This is currently used to send timing
// information about cross-process iframes for window.performance. The
// browser-side equivalent to this struct is content::ResourceTimingInfo.
// Note: Please update operator==() and CrossThreadCopier whenever a new field
// is added.
// TODO(dcheng): Migrate this struct over to Mojo so it doesn't need to be
// duplicated in //content and //third_party/blink.
struct WebResourceTimingInfo {
#if INSIDE_BLINK
  PLATFORM_EXPORT bool operator==(const WebResourceTimingInfo&) const;
#endif

  // The name to associate with the performance entry. For iframes, this is
  // typically the initial URL of the iframe resource.
  WebString name;
  base::TimeTicks start_time;

  WebString alpn_negotiated_protocol;
  WebString connection_info;

  WebURLLoadTiming timing;
  base::TimeTicks last_redirect_end_time;
  base::TimeTicks response_end;

  uint64_t transfer_size = 0U;
  uint64_t encoded_body_size = 0U;
  uint64_t decoded_body_size = 0U;

  bool did_reuse_connection = false;
  bool is_secure_context = false;

  // TODO(dcheng): The way this code works is fairly confusing: it might seem
  // unusual to store policy members like |allow_timing_details| inline, rather
  // than just clearing the fields. The reason for this complexity is because
  // PerformanceNavigationTiming inherits and shares many of the same fields
  // exposed by PerformanceResourceTiming, but the underlying behavior is a
  // little different.
  bool allow_timing_details = false;
  bool allow_redirect_details = false;

  // Normally, the timestamps are relative to the time origin. In most cases,
  // these timestamps should be positive value, so 0 is used to mark invalid
  // negative values.
  //
  // However, ServiceWorker navigation preloads may be negative, since these
  // requests may be started before the service worker started. In those cases,
  // this flag should be set to true.
  bool allow_negative_values = false;

  WebVector<WebServerTimingInfo> server_timing;
};

}  // namespace blink

namespace WTF {
#if INSIDE_BLINK
template <>
struct CrossThreadCopier<blink::WebResourceTimingInfo> {
  STATIC_ONLY(CrossThreadCopier);
  typedef blink::WebResourceTimingInfo Type;
  PLATFORM_EXPORT static Type Copy(const blink::WebResourceTimingInfo&);
};
#endif
}  // namespace WTF

#endif
