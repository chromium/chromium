// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_PLATFORM_API_UTIL_H_
#define NET_BASE_NET_PLATFORM_API_UTIL_H_

#include <string_view>

#include "base/containers/span.h"
#include "net/base/net_export.h"

namespace net {

// Copies a string_view to the provided span, adding a terminating '\0'. Does
// not zero-fill the rest of the span. `dest` must be long enough. `span` does
// not need to be null terminated. CHECKs if the provided span, including the
// nul, won't fit in `dest`. This is useful to use with platform APIs that take
// C-style strings in struct fields. To use this in that case:
//   `CopyStringToSpanWithNul(string, std::span(struct.c_string_field))`
NET_EXPORT_PRIVATE void CopyStringAndNulToSpan(std::string_view src,
                                               base::span<char> dest);

// Does the opposite of the above method, for extracting strings from platform
// structs. Finds the first nul in `span`, and returns a string_view containing
// all characters up to (but not including) the nul. If there is no nul at the
// end of the span, returns the entire span, as a string_view.
//
// To use this with a platform API that returns a struct with a C-string:
//   `SpanWithNulToString(std::span(struct.c_string_field))`
NET_EXPORT_PRIVATE std::string_view SpanMaybeWithNulToStringView(
    base::span<const char> span);

}  // namespace net

#endif  // NET_BASE_NET_PLATFORM_API_UTIL_H_
