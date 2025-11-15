// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CROSS_THREAD_COPIER_URL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CROSS_THREAD_COPIER_URL_H_

#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "url/scheme_host_port.h"

namespace network {
struct URLLoaderCompletionStatus;
}

namespace blink {

template <>
struct CrossThreadCopier<url::SchemeHostPort> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = url::SchemeHostPort;
  static Type Copy(const Type& value) { return value; }
};

template <>
struct CrossThreadCopier<network::URLLoaderCompletionStatus>
    : public CrossThreadCopierPassThrough<network::URLLoaderCompletionStatus> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CROSS_THREAD_COPIER_URL_H_
