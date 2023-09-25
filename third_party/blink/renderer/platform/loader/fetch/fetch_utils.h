// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_

#include "net/traffic_annotation/network_traffic_annotation.h"
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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_
