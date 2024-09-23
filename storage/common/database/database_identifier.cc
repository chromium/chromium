// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/common/database/database_identifier.h"

#include <stddef.h>

#include <string>

#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon.h"
#include "url/url_features.h"

namespace storage {

namespace {

// If the passed string is of the form "[1::2:3]", returns "[1__2_3]".
std::string EscapeIPv6Hostname(const std::string& hostname) {
  // Shortest IPv6 hostname would be "[::1]".
  if (hostname.size() < 5 || hostname.front() != '[' || hostname.back() != ']')
    return hostname;

  // Should be canonicalized before it gets this far.
  // i.e. "[::ffff:8190:3426]" not "[::FFFF:129.144.52.38]"
  DCHECK(base::ContainsOnlyChars(hostname, "[]:0123456789abcdef"));

  std::string copy = hostname;
  base::ReplaceChars(hostname, ":", "_", &copy);
  return copy;
}

// If the passed string is of the form "[1__2_3]", returns "[1::2:3]".
std::string UnescapeIPv6Hostname(const std::string& hostname) {
  if (hostname.size() < 5 || hostname.front() != '[' || hostname.back() != ']')
    return hostname;

  std::string copy = hostname;
  base::ReplaceChars(hostname, "_", ":", &copy);
  return copy;
}

static bool SchemeIsUnique(const std::string& scheme) {
  return scheme == "about" || scheme == "data" || scheme == "javascript";
}

class DatabaseIdentifier {
 public:
  static const DatabaseIdentifier UniqueFileIdentifier();
  static DatabaseIdentifier CreateFromOrigin(const GURL& origin);
  static DatabaseIdentifier Parse(const std::string& identifier);
  ~DatabaseIdentifier();

  std::string ToString() const;
  GURL ToOrigin() const;

 private:
  DatabaseIdentifier();
  DatabaseIdentifier(const std::string& scheme,
                     const std::string& hostname,
                     int port,
                     bool is_unique,
                     bool is_file);

  std::string scheme_;
  std::string hostname_;
  int port_;
  bool is_unique_;
  bool is_file_;
};

// static
const DatabaseIdentifier DatabaseIdentifier::UniqueFileIdentifier() {
  return DatabaseIdentifier("", "", 0, true, true);
}

// static
DatabaseIdentifier DatabaseIdentifier::CreateFromOrigin(const GURL& origin) {
  if (!origin.is_valid() || origin.is_empty() || !origin.IsStandard() ||
      SchemeIsUnique(origin.scheme())) {
    return DatabaseIdentifier();
  }

  if (origin.SchemeIsFile())
    return UniqueFileIdentifier();

  int port = origin.IntPort();
  if (port == url::PORT_INVALID)
    return DatabaseIdentifier();

  // We encode the default port for the specified scheme as 0. GURL
  // canonicalizes this as an unspecified port.
  if (port == url::PORT_UNSPECIFIED)
    port = 0;

  return DatabaseIdentifier(origin.scheme(),
                            origin.host(),
                            port,
                            false /* unique */,
                            false /* file */);
}

// static
DatabaseIdentifier DatabaseIdentifier::Parse(const std::string& identifier) {
  if (!base::IsStringASCII(identifier))
    return DatabaseIdentifier();
  if (base::Contains(identifier, "..")) {
    return DatabaseIdentifier();
  }
  static const char kForbidden[] = {'\\', '/', ':', '\0'};
  if (identifier.find_first_of(kForbidden, 0, std::size(kForbidden)) !=
      std::string::npos) {
    return DatabaseIdentifier();
  }

  size_t first_underscore = identifier.find_first_of('_');
  if (first_underscore == std::string::npos || first_underscore == 0)
    return DatabaseIdentifier();

  size_t last_underscore = identifier.find_last_of('_');
  if (last_underscore == std::string::npos ||
      last_underscore == first_underscore ||
      last_underscore == identifier.size() - 1) {
    return DatabaseIdentifier();
  }

  std::string scheme(identifier.data(), first_underscore);
  if (scheme == "file")
    return UniqueFileIdentifier();

  // This magical set of schemes is always treated as unique.
  if (SchemeIsUnique(scheme))
    return DatabaseIdentifier();

  auto port_str = base::MakeStringPiece(
      identifier.begin() + last_underscore + 1, identifier.end());
  int port = 0;
  constexpr int kMaxPort = 65535;
  if (!base::StringToInt(port_str, &port) || port < 0 || port > kMaxPort)
    return DatabaseIdentifier();

  std::string hostname =
      UnescapeIPv6Hostname(std::string(identifier.data() + first_underscore + 1,
                                       last_underscore - first_underscore - 1));

  GURL url(scheme + "://" + hostname + "/");

  // If a url doesn't parse cleanly or doesn't round trip, reject it.
  if (!url.is_valid() || url.scheme() != scheme) {
    return DatabaseIdentifier();
  }
  // Unless StandardCompliantNonSpecialSchemeURLParsing feature is enabled,
  // url.host() always return an empty string for non-special URLs.
  if ((url::IsUsingStandardCompliantNonSpecialSchemeURLParsing() ||
       url.IsStandard()) &&
      (url.host() != hostname)) {
    return DatabaseIdentifier();
  }
  // Clear hostname for a non-special URL. This behavior existed before
  // non-special URLs are properly supported, and we're keeping this for
  // compatibility reasons.
  if (!url.IsStandard()) {
    hostname.clear();
  }
  return DatabaseIdentifier(scheme, hostname, port, false /* unique */, false);
}

DatabaseIdentifier::DatabaseIdentifier()
    : port_(0),
      is_unique_(true),
      is_file_(false) {
}

DatabaseIdentifier::DatabaseIdentifier(const std::string& scheme,
                                       const std::string& hostname,
                                       int port,
                                       bool is_unique,
                                       bool is_file)
    : scheme_(scheme),
      hostname_(base::ToLowerASCII(hostname)),
      port_(port),
      is_unique_(is_unique),
      is_file_(is_file) {
}

DatabaseIdentifier::~DatabaseIdentifier() = default;

std::string DatabaseIdentifier::ToString() const {
  if (is_file_)
    return "file__0";
  if (is_unique_)
    return "__0";
  return scheme_ + "_" + EscapeIPv6Hostname(hostname_) + "_" +
         base::NumberToString(port_);
}

GURL DatabaseIdentifier::ToOrigin() const {
  if (is_file_)
    return GURL("file:///");
  if (is_unique_)
    return GURL();
  if (port_ == 0)
    return GURL(scheme_ + "://" + hostname_);
  return GURL(scheme_ + "://" + hostname_ + ":" + base::NumberToString(port_));
}

}  // namespace

// static
std::string GetIdentifierFromOrigin(const GURL& origin) {
  return DatabaseIdentifier::CreateFromOrigin(origin).ToString();
}

// static
std::string GetIdentifierFromOrigin(const url::Origin& origin) {
  return GetIdentifierFromOrigin(origin.GetURL());
}

// static
url::Origin GetOriginFromIdentifier(const std::string& identifier) {
  return url::Origin::Create(DatabaseIdentifier::Parse(identifier).ToOrigin());
}

// static
GURL GetOriginURLFromIdentifier(const std::string& identifier) {
  return DatabaseIdentifier::Parse(identifier).ToOrigin();
}

// static
bool IsValidOriginIdentifier(const std::string& identifier) {
  return GetOriginURLFromIdentifier(identifier).is_valid();
}

}  // namespace storage
