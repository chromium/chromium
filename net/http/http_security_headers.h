// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SECURITY_HEADERS_H_
#define NET_HTTP_HTTP_SECURITY_HEADERS_H_

#include <stdint.h>

#include <string_view>

#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"

namespace net {

const uint32_t kMaxHSTSAgeSecs = 86400 * 365;  // 1 year

// RFC7469 suggests that 60 days is a reasonable maximum max-age value
// http://tools.ietf.org/html/rfc7469#section-4.1
const uint32_t kMaxHPKPAgeSecs = 86400 * 60;  // 60 days

// Parses |value| as a Strict-Transport-Security header value. If successful,
// returns true and sets |*max_age| and |*include_subdomains|.
// Otherwise returns false and leaves the output parameters unchanged.
//
// value is the right-hand side of:
//
// "Strict-Transport-Security" ":"
//     [ directive ]  *( ";" [ directive ] )
bool NET_EXPORT_PRIVATE ParseHSTSHeader(std::string_view value,
                                        base::TimeDelta* max_age,
                                        bool* include_subdomains);

}  // namespace net

#endif  // NET_HTTP_HTTP_SECURITY_HEADERS_H_
