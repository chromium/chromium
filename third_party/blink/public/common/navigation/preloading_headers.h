// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_PRELOADING_HEADERS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_PRELOADING_HEADERS_H_

#include <optional>
#include <string>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// The header used to indicate the purpose of a request. For more info see
// https://wicg.github.io/nav-speculation/prefetch.html#sec-purpose-header
//
// One of the header values defined below should be used when setting this
// header in Chromium.
//
// When adding a new value below, maybe `IsSecPurposeForPrefetch` should be also
// updated.
inline constexpr char kSecPurposeHeaderName[] = "Sec-Purpose";

// Returns true if the given `Sec-Purpose` request header value is for prefetch.
// Note: this assumes the header value is set by Chromium implementation using
// the header values below, as this method doesn't perform full structured
// header value parsing.
BLINK_COMMON_EXPORT bool IsSecPurposeForPrefetch(
    std::optional<std::string> sec_purpose_header_value);

// Returns true if the given `Sec-Purpose` request header value is for
// prerender. Note: this assumes the header value is set by Chromium
// implementation using the header value
// `kSecPurposePrefetchPrerenderHeaderValue` below, as this method doesn't
// perform full structured header value parsing.
BLINK_COMMON_EXPORT bool IsSecPurposeForPrerender(
    std::optional<std::string> sec_purpose_header_value);

// This value indicates that the request is a prefetch request made directly to
// the server.
inline constexpr char kSecPurposePrefetchHeaderValue[] = "prefetch";

// This value indicates that the request is a prefetch request made via an
// anonymous client IP proxy.
inline constexpr char kSecPurposePrefetchAnonymousClientIpHeaderValue[] =
    "prefetch;anonymous-client-ip";

// This value indicates that the request is a prerender request.
inline constexpr char kSecPurposePrefetchPrerenderHeaderValue[] =
    "prefetch;prerender";

// This value indicates that the request is a preview request.
inline constexpr char kSecPurposePrefetchPrerenderPreviewHeaderValue[] =
    "prefetch;prerender;preview";

// The Chromium specific header equivalent for 'Sec-Purpose':
inline constexpr char kPurposeHeaderName[] = "Purpose";

// For more info see
// https://wicg.github.io/nav-speculation/prefetch.html#sec-speculation-tags-header
inline constexpr char kSecSpeculationTagsHeaderName[] = "Sec-Speculation-Tags";

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_PRELOADING_HEADERS_H_
