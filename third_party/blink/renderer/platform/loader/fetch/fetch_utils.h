// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_

#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace WTF {
class AtomicString;
class String;
}  // namespace WTF

namespace blink {

class PLATFORM_EXPORT FetchUtils {
  STATIC_ONLY(FetchUtils);

 public:
  static bool IsForbiddenMethod(const WTF::String& method);
  static bool IsForbiddenResponseHeaderName(const WTF::String& name);
  static WTF::AtomicString NormalizeMethod(const WTF::AtomicString& method);
  static WTF::String NormalizeHeaderValue(const WTF::String& value);

  static net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag(
      const network::ResourceRequest& request);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_
