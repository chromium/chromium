// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_TYPE_H_
#define CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_TYPE_H_

#include "content/common/content_export.h"

namespace content {

// The type of prefetch. This determines various details about how a prefetch is
// handled.
class CONTENT_EXPORT PrefetchType {
 public:
  PrefetchType(bool use_isolated_network_context, bool use_prefetch_proxy);
  ~PrefetchType();

  PrefetchType(const PrefetchType& prefetch_type);
  PrefetchType& operator=(const PrefetchType& prefetch_type);

  // Whether prefetches of this type need to use an isolated network context, or
  // use the default network context.
  bool IsIsolatedNetworkContextRequired() const {
    return use_isolated_network_context_;
  }

  // Whether prefetches of this type need to use the Prefetch Proxy.
  bool IsProxyRequired() const { return use_prefetch_proxy_; }

 private:
  friend CONTENT_EXPORT bool operator==(const PrefetchType& prefetch_type_1,
                                        const PrefetchType& prefetch_type_2);

  bool use_isolated_network_context_;
  bool use_prefetch_proxy_;
};

CONTENT_EXPORT bool operator==(const PrefetchType& prefetch_type_1,
                               const PrefetchType& prefetch_type_2);
CONTENT_EXPORT bool operator!=(const PrefetchType& prefetch_type_1,
                               const PrefetchType& prefetch_type_2);

}  // namespace content

#endif  // CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_TYPE_H_
