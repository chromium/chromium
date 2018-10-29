// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_NETWORK_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_NETWORK_UTILS_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class KURL;
class SharedBuffer;
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

// Returns the decoded data url as ResourceResponse and SharedBuffer
// if url had a supported mimetype and parsing was successful.
PLATFORM_EXPORT scoped_refptr<SharedBuffer> ParseDataURLAndPopulateResponse(
    const KURL&,
    ResourceResponse&);

// Returns true if the URL is a data URL and its MIME type is in the list of
// supported/recognized MIME types.
PLATFORM_EXPORT bool IsDataURLMimeTypeSupported(const KURL&);

PLATFORM_EXPORT bool IsRedirectResponseCode(int);

PLATFORM_EXPORT bool IsCertificateTransparencyRequiredError(int);

PLATFORM_EXPORT bool IsLegacySymantecCertError(int);

PLATFORM_EXPORT String GenerateAcceptLanguageHeader(const String&);

}  // namespace network_utils

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_NETWORK_UTILS_H_
