// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/effective_connection_type_helper.h"

#include "third_party/blink/public/common/client_hints/client_hints.h"

namespace content {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

STATIC_ASSERT_ENUM(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
                   blink::WebEffectiveConnectionType::kTypeUnknown);
STATIC_ASSERT_ENUM(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
                   blink::WebEffectiveConnectionType::kTypeOffline);
STATIC_ASSERT_ENUM(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
                   blink::WebEffectiveConnectionType::kTypeSlow2G);
STATIC_ASSERT_ENUM(net::EFFECTIVE_CONNECTION_TYPE_2G,
                   blink::WebEffectiveConnectionType::kType2G);
STATIC_ASSERT_ENUM(net::EFFECTIVE_CONNECTION_TYPE_3G,
                   blink::WebEffectiveConnectionType::kType3G);
STATIC_ASSERT_ENUM(net::EFFECTIVE_CONNECTION_TYPE_4G,
                   blink::WebEffectiveConnectionType::kType4G);

#undef STATIC_ASSERT_ENUM

static_assert(net::EFFECTIVE_CONNECTION_TYPE_4G + 1 ==
                  net::EFFECTIVE_CONNECTION_TYPE_LAST,
              "When adding a new effective connection type, "
              "WebEffectiveConnectionType.h should be updated too");

static_assert(static_cast<int>(blink::WebEffectiveConnectionType::kType4G) +
                      1 ==
                  net::EFFECTIVE_CONNECTION_TYPE_LAST,
              "When adding a new effective connection type, "
              "WebEffectiveConnectionType.h should be updated too");

blink::WebEffectiveConnectionType
EffectiveConnectionTypeToWebEffectiveConnectionType(
    net::EffectiveConnectionType net_type) {
  return static_cast<blink::WebEffectiveConnectionType>(net_type);
}

}  // namespace content
