// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_port_pair.h"

#include <optional>
#include <string_view>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

// Value dictionary keys
constexpr std::string_view kValueHostKey = "host";
constexpr std::string_view kValuePortKey = "port";

}  // namespace

HostPortPair::HostPortPair() : port_(0) {}
HostPortPair::HostPortPair(std::string_view in_host, uint16_t in_port)
    : host_(in_host), port_(in_port) {}

// static
HostPortPair HostPortPair::FromURL(const GURL& url) {
  return HostPortPair(url.HostNoBrackets(),
                      static_cast<uint16_t>(url.EffectiveIntPort()));
}

// static
HostPortPair HostPortPair::FromSchemeHostPort(
    const url::SchemeHostPort& scheme_host_port) {
  DCHECK(scheme_host_port.IsValid());

  // HostPortPair assumes hostnames do not have surrounding brackets (as is
  // commonly used for IPv6 literals), so strip them if present.
  std::string_view host = scheme_host_port.host();
  if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
    host = host.substr(1, host.size() - 2);
  }

  return HostPortPair(host, scheme_host_port.port());
}

// static
HostPortPair HostPortPair::FromIPEndPoint(const IPEndPoint& ipe) {
  return HostPortPair(ipe.ToStringWithoutPort(), ipe.port());
}

// static
HostPortPair HostPortPair::FromString(std::string_view str) {
  // Input with more than one ':' is ambiguous unless it contains an IPv6
  // literal (signified by starting with a '['). ParseHostAndPort() allows such
  // input and always uses the last ':' as the host/port delimiter, but because
  // HostPortPair often deals with IPv6 literals without brackets, disallow such
  // input here to prevent a common error.
  if (base::SplitStringPiece(str, ":", base::KEEP_WHITESPACE,
                             base::SPLIT_WANT_ALL)
              .size() > 2 &&
      str.front() != '[') {
    return HostPortPair();
  }

  std::string host;
  int port;
  if (!ParseHostAndPort(str, &host, &port))
    return HostPortPair();

  // Require a valid port.
  if (port == -1)
    return HostPortPair();
  DCHECK(base::IsValueInRangeForNumericType<uint16_t>(port));

  return HostPortPair(host, port);
}

// static
std::optional<HostPortPair> HostPortPair::FromValue(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict)
    return std::nullopt;

  const std::string* host = dict->FindString(kValueHostKey);
  std::optional<int> port = dict->FindInt(kValuePortKey);

  if (host == nullptr || !port.has_value() ||
      !base::IsValueInRangeForNumericType<uint16_t>(port.value())) {
    return std::nullopt;
  }

  return HostPortPair(*host, base::checked_cast<uint16_t>(port.value()));
}

std::string HostPortPair::ToString() const {
  std::string ret(HostForURL());
  ret += ':';
  ret += base::NumberToString(port_);
  return ret;
}

std::string HostPortPair::HostForURL() const {
  // TODO(rtenneti): Add support for |host| to have '\0'.
  if (host_.find('\0') != std::string::npos) {
    std::string host_for_log(host_);
    size_t nullpos;
    while ((nullpos = host_for_log.find('\0')) != std::string::npos) {
      host_for_log.replace(nullpos, 1, "%00");
    }
    LOG(DFATAL) << "Host has a null char: " << host_for_log;
  }
  // Check to see if the host is an IPv6 address.  If so, added brackets.
  if (host_.find(':') != std::string::npos) {
    DCHECK_NE(host_[0], '[');
    return base::StringPrintf("[%s]", host_.c_str());
  }

  return host_;
}

base::Value HostPortPair::ToValue() const {
  base::Value::Dict dict;
  dict.Set(kValueHostKey, host_);
  dict.Set(kValuePortKey, port_);

  return base::Value(std::move(dict));
}

}  // namespace net
