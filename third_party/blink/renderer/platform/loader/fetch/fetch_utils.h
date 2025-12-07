// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_

#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace blink {
class PLATFORM_EXPORT FetchUtils {
  STATIC_ONLY(FetchUtils);

 public:
  static bool IsForbiddenMethod(const String& method);
  static bool IsForbiddenResponseHeaderName(const String& name);
  static AtomicString NormalizeMethod(const AtomicString& method);
  static String NormalizeHeaderValue(const String& value);

  static net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag(
      const network::ResourceRequest& request);

  // The state of the fetch keepalive request to log.
  enum class FetchKeepAliveRequestState {
    kTotal = 0,
    kStarted = 1,
    kSucceeded = 2,
    kFailed = 3,
  };
  // A shared function to help log fetch keepalive requests related UMAs.
  // Note that FetchLater requests must not be logged by this method.
  // See the following doc for more details:
  // https://docs.google.com/document/d/15MHmkf_SN2S9WYra060yEChgjs3pgZW--aHUuiG8Y1Q/edit#heading=h.z4xv4ogkdxqw
  static void LogFetchKeepAliveRequestMetric(
      const mojom::blink::RequestContextType&,
      const FetchKeepAliveRequestState&,
      bool is_context_detached = false);
  static void LogFetchKeepAliveRequestSentToServiceMetric(
      const network::ResourceRequest& resource_request);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_
