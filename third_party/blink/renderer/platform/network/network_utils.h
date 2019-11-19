// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_NETWORK_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_NETWORK_UTILS_H_

#include <string>
#include <tuple>

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class KURL;
class ResourceResponse;

namespace network_utils {

enum PrivateRegistryFilter {
  kIncludePrivateRegistries,
  kExcludePrivateRegistries,
};

PLATFORM_EXPORT bool IsReservedIPAddress(const String& host);

PLATFORM_EXPORT bool IsLocalHostname(const String& host, bool* is_local6);

PLATFORM_EXPORT String GetDomainAndRegistry(const String& host,
                                            PrivateRegistryFilter);

// Returns the decoded data url as ResourceResponse and SharedBuffer if parsing
// was successful. The result is returned as net error code. It returns net::OK
// if decoding succeeds, otherwise it failed.
PLATFORM_EXPORT std::tuple<int, ResourceResponse, scoped_refptr<SharedBuffer>>
ParseDataURL(const KURL&, const String& method);

// Returns true if the URL is a data URL and its MIME type is in the list of
// supported/recognized MIME types.
PLATFORM_EXPORT bool IsDataURLMimeTypeSupported(
    const KURL&,
    std::string* data = nullptr,
    std::string* mime_type = nullptr);

PLATFORM_EXPORT bool IsRedirectResponseCode(int);

PLATFORM_EXPORT bool IsCertificateTransparencyRequiredError(int);

PLATFORM_EXPORT String GenerateAcceptLanguageHeader(const String&);

PLATFORM_EXPORT Vector<char> ParseMultipartBoundary(
    const AtomicString& content_type_header);

}  // namespace network_utils

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_NETWORK_UTILS_H_
