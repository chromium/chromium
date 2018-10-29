// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/tools/quic_url.h"

#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"

namespace quic {

static constexpr size_t kMaxHostNameLength = 256;

QuicUrl::QuicUrl(QuicStringPiece url) : url_(static_cast<QuicString>(url)) {}

QuicUrl::QuicUrl(QuicStringPiece url, QuicStringPiece default_scheme)
    : QuicUrl(url) {
  if (url_.has_scheme()) {
    return;
  }

  url_ = GURL(QuicStrCat(default_scheme, "://", url));
}

QuicString QuicUrl::ToString() const {
  if (IsValid()) {
    return url_.spec();
  }
  return "";
}

bool QuicUrl::IsValid() const {
  if (!url_.is_valid() || !url_.has_scheme()) {
    return false;
  }

  if (url_.has_host() && url_.host().length() > kMaxHostNameLength) {
    return false;
  }

  return true;
}

QuicString QuicUrl::HostPort() const {
  if (!IsValid() || !url_.has_host()) {
    return "";
  }

  QuicString host = url_.host();
  int port = url_.IntPort();
  if (port == url::PORT_UNSPECIFIED) {
    return host;
  }
  return QuicStrCat(host, ":", port);
}

QuicString QuicUrl::PathParamsQuery() const {
  if (!IsValid() || !url_.has_path()) {
    return "/";
  }

  return url_.PathForRequest();
}

QuicString QuicUrl::scheme() const {
  if (!IsValid()) {
    return "";
  }

  return url_.scheme();
}

QuicString QuicUrl::host() const {
  if (!IsValid()) {
    return "";
  }

  return url_.HostNoBrackets();
}

QuicString QuicUrl::path() const {
  if (!IsValid()) {
    return "";
  }

  return url_.path();
}

uint16_t QuicUrl::port() const {
  if (!IsValid()) {
    return 0;
  }

  int port = url_.EffectiveIntPort();
  if (port == url::PORT_UNSPECIFIED) {
    return 0;
  }
  return port;
}

}  // namespace quic
