// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_port_pair.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/ip_endpoint.h"
#include "net/base/parse_number.h"
#include "net/base/port_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

HostPortPair::HostPortPair() : port_(0) {}
HostPortPair::HostPortPair(base::StringPiece in_host, uint16_t in_port)
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
  base::StringPiece host = scheme_host_port.host();
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
HostPortPair HostPortPair::FromString(const std::string& str) {
  std::vector<base::StringPiece> key_port = base::SplitStringPiece(
      str, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (key_port.size() != 2)
    return HostPortPair();
  int port;
  if (!ParseInt32(key_port[1], ParseIntFormat::NON_NEGATIVE, &port))
    return HostPortPair();
  if (!IsPortValid(port))
    return HostPortPair();
  HostPortPair host_port_pair;
  host_port_pair.set_host(std::string(key_port[0]));
  host_port_pair.set_port(static_cast<uint16_t>(port));
  return host_port_pair;
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

size_t HostPortPair::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(host_);
}

}  // namespace net
