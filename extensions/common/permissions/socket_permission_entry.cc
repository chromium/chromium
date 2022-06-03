// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/socket_permission_entry.h"

#include <stdint.h>

#include <cstdlib>
#include <memory>
#include <sstream>
#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/socket_permission.h"
#include "url/url_canon.h"

namespace {

using content::SocketPermissionRequest;

const char kColon = ':';
const char kDot = '.';
const char kWildcard[] = "*";
const uint16_t kWildcardPortNumber = 0;
const uint16_t kInvalidPort = 65535;

bool StartsOrEndsWithWhitespace(const std::string& str) {
  return !str.empty() && (base::IsUnicodeWhitespace(str.front()) ||
                          base::IsUnicodeWhitespace(str.back()));
}

}  // namespace

namespace extensions {

SocketPermissionEntry::SocketPermissionEntry()
    : pattern_(SocketPermissionRequest::NONE, std::string(), kInvalidPort),
      match_subdomains_(false) {}

SocketPermissionEntry::~SocketPermissionEntry() {}

bool SocketPermissionEntry::operator<(const SocketPermissionEntry& rhs) const {
  return std::tie(pattern_.type, pattern_.host, match_subdomains_,
                  pattern_.port) <
         std::tie(rhs.pattern_.type, rhs.pattern_.host, rhs.match_subdomains_,
                  rhs.pattern_.port);
}

bool SocketPermissionEntry::operator==(const SocketPermissionEntry& rhs) const {
  return (pattern_.type == rhs.pattern_.type) &&
         (pattern_.host == rhs.pattern_.host) &&
         (match_subdomains_ == rhs.match_subdomains_) &&
         (pattern_.port == rhs.pattern_.port);
}

bool SocketPermissionEntry::Check(
    const content::SocketPermissionRequest& request) const {
  if (pattern_.type != request.type)
    return false;

  std::string lhost = base::ToLowerASCII(request.host);
  if (pattern_.host != lhost) {
    if (!match_subdomains_)
      return false;

    if (!pattern_.host.empty()) {
      // Do not wildcard part of IP address.
      url::Component component(0, lhost.length());
      url::RawCanonOutputT<char, 128> ignored_output;
      url::CanonHostInfo host_info;
      url::CanonicalizeIPAddress(
          lhost.c_str(), component, &ignored_output, &host_info);
      if (host_info.IsIPAddress())
        return false;

      // host should equal one or more chars + "." +  host_.
      int i = lhost.length() - pattern_.host.length();
      if (i < 2)
        return false;

      if (lhost.compare(i, pattern_.host.length(), pattern_.host) != 0)
        return false;

      if (lhost[i - 1] != kDot)
        return false;
    }
  }

  if (pattern_.port != request.port && pattern_.port != kWildcardPortNumber)
    return false;

  return true;
}

SocketPermissionEntry::HostType SocketPermissionEntry::GetHostType() const {
  return pattern_.host.empty()
             ? SocketPermissionEntry::ANY_HOST
             : match_subdomains_ ? SocketPermissionEntry::HOSTS_IN_DOMAINS
                                 : SocketPermissionEntry::SPECIFIC_HOSTS;
}

bool SocketPermissionEntry::IsAddressBoundType() const {
  return pattern_.type == SocketPermissionRequest::TCP_CONNECT ||
         pattern_.type == SocketPermissionRequest::TCP_LISTEN ||
         pattern_.type == SocketPermissionRequest::UDP_BIND ||
         pattern_.type == SocketPermissionRequest::UDP_SEND_TO;
}

// static
bool SocketPermissionEntry::ParseHostPattern(
    SocketPermissionRequest::OperationType type,
    const std::string& pattern,
    SocketPermissionEntry* entry) {
  std::vector<std::string> tokens =
      base::SplitString(pattern, std::string(1, kColon), base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  return ParseHostPattern(type, tokens, entry);
}

// static
bool SocketPermissionEntry::ParseHostPattern(
    SocketPermissionRequest::OperationType type,
    const std::vector<std::string>& pattern_tokens,
    SocketPermissionEntry* entry) {

  SocketPermissionEntry result;

  if (type == SocketPermissionRequest::NONE)
    return false;

  if (pattern_tokens.size() > 2)
    return false;

  result.pattern_.type = type;
  result.pattern_.port = kWildcardPortNumber;
  result.match_subdomains_ = true;

  if (pattern_tokens.size() == 0) {
    *entry = result;
    return true;
  }

  // Return an error if address is specified for permissions that don't
  // need it (such as 'resolve-host').
  if (!result.IsAddressBoundType())
    return false;

  result.pattern_.host = pattern_tokens[0];
  if (!result.pattern_.host.empty()) {
    if (StartsOrEndsWithWhitespace(result.pattern_.host))
      return false;
    result.pattern_.host = base::ToLowerASCII(result.pattern_.host);

    // The first component can optionally be '*' to match all subdomains.
    std::vector<base::StringPiece> host_components =
        base::SplitStringPiece(result.pattern_.host, std::string{kDot},
                               base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    DCHECK(!host_components.empty());

    if (host_components[0] == kWildcard || host_components[0].empty()) {
      host_components.erase(host_components.begin(),
                            host_components.begin() + 1);
    } else {
      result.match_subdomains_ = false;
    }
    result.pattern_.host = base::JoinString(host_components, ".");
  }

  if (pattern_tokens.size() == 1 || pattern_tokens[1].empty() ||
      pattern_tokens[1] == kWildcard) {
    *entry = result;
    return true;
  }

  if (StartsOrEndsWithWhitespace(pattern_tokens[1]))
    return false;

  int port;
  if (!base::StringToInt(pattern_tokens[1], &port) || port < 1 || port > 65535)
    return false;
  result.pattern_.port = static_cast<uint16_t>(port);

  *entry = result;
  return true;
}

std::string SocketPermissionEntry::GetHostPatternAsString() const {
  std::string result;

  if (!IsAddressBoundType())
    return result;

  if (match_subdomains()) {
    result.append(kWildcard);
    if (!pattern_.host.empty())
      result.append(1, kDot).append(pattern_.host);
  } else {
    result.append(pattern_.host);
  }

  if (pattern_.port == kWildcardPortNumber)
    result.append(1, kColon).append(kWildcard);
  else
    result.append(1, kColon).append(base::NumberToString(pattern_.port));

  return result;
}

}  // namespace extensions
