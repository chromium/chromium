// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_NETWORK_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_NETWORK_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "net/http/http_response_headers.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {
namespace network_utils {

// Returns true if the headers indicate that this resource should always be
// revalidated or not cached.
BLINK_COMMON_EXPORT bool AlwaysAccessNetwork(
    const scoped_refptr<net::HttpResponseHeaders>& headers);

}  // namespace network_utils
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_NETWORK_UTILS_H_
