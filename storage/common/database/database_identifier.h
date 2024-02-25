// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_DATABASE_DATABASE_IDENTIFIER_H_
#define STORAGE_COMMON_DATABASE_DATABASE_IDENTIFIER_H_

#include <string>

#include "base/component_export.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace storage {

// Invokes the version of GetIdentifierFromOrigin() below with the result of
// `origin.GetURL()`.
COMPONENT_EXPORT(STORAGE_COMMON)
std::string GetIdentifierFromOrigin(const url::Origin& origin);
COMPONENT_EXPORT(STORAGE_COMMON)
url::Origin GetOriginFromIdentifier(const std::string& identifier);

// Checks that the `GURL` passed in is a valid parsed URL i.e. a UTF-8 string
// with any non-ASCII characters percent encoded to ASCII.
// For valid GURLs, it returns an ASCII string containing the URL's scheme,
// host, and port separated by underscores (_). In case of IPv6 hostnames, it
// replaces colons (:) with underscores (_).
// For valid GURLs with file schemes (file: and filesystem:), it returns
// "file__0".
// For invalid URLs and non standard schemes, it returns "__0".
//
// TODO(jsbell): Remove use of the GURL variants.
COMPONENT_EXPORT(STORAGE_COMMON)
std::string GetIdentifierFromOrigin(const GURL& origin);
COMPONENT_EXPORT(STORAGE_COMMON)
GURL GetOriginURLFromIdentifier(const std::string& identifier);

COMPONENT_EXPORT(STORAGE_COMMON)
bool IsValidOriginIdentifier(const std::string& identifier);

}  // namespace storage

#endif  // STORAGE_COMMON_DATABASE_DATABASE_IDENTIFIER_H_
