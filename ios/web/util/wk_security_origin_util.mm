// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/util/wk_security_origin_util.h"

#import <WebKit/WebKit.h>

#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "url/gurl.h"
#import "url/origin.h"
#import "url/scheme_host_port.h"

namespace web {

GURL GURLOriginWithWKSecurityOrigin(WKSecurityOrigin* origin) {
  if (!origin) {
    return GURL();
  }
  std::string scheme = base::SysNSStringToUTF8(origin.protocol);
  std::string host = base::SysNSStringToUTF8(origin.host);
  uint16_t port = base::checked_cast<uint16_t>(origin.port);
  if (port == 0) {
    // WKSecurityOrigin.port is 0 if the effective port of this origin is the
    // default for its scheme.
    int default_port = url::DefaultPortForScheme(scheme);
    if (default_port != url::PORT_UNSPECIFIED) {
      port = base::checked_cast<uint16_t>(default_port);
    }
  }

  url::SchemeHostPort origin_tuple(scheme, host, port);
  return origin_tuple.GetURL();
}

url::Origin OriginWithWKSecurityOrigin(WKSecurityOrigin* origin) {
  if (!origin) {
    return url::Origin();
  }
  std::string scheme = base::SysNSStringToUTF8(origin.protocol);
  std::string host = base::SysNSStringToUTF8(origin.host);
  uint16_t port = base::checked_cast<uint16_t>(origin.port);
  if (port == 0) {
    // WKSecurityOrigin.port is 0 if the effective port of this origin is the
    // default for its scheme.
    int default_port = url::DefaultPortForScheme(scheme);
    if (default_port != url::PORT_UNSPECIFIED) {
      port = base::checked_cast<uint16_t>(default_port);
    }
  }

  std::optional<url::Origin> parsed_origin =
      url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(scheme, host,
                                                                 port);
  if (!parsed_origin) {
    return url::Origin();
  }
  return parsed_origin.value();
}

}  // namespace web
