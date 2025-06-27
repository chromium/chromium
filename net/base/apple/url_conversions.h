// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_APPLE_URL_CONVERSIONS_H_
#define NET_BASE_APPLE_URL_CONVERSIONS_H_

#include "net/base/net_export.h"

class GURL;
@class NSURL;

namespace net {

// Method for creating a valid NSURL (compliant with RFC 1738/1808/2396) from a
// valid GURL. This method will return nil if `url.is_valid() == false`.
// As GURL and NSURL valid URL sets do not match, it is possible that this
// function returns nil even if `url.is_valid() == true`. The result must always
// be checked for nullity.
// Note that NSURLs should *always* be created from GURLs, so that GURL
// sanitization rules are applied everywhere.
NET_EXPORT NSURL* NSURLWithGURL(const GURL& url);

// Method for creating a valid GURL from a NSURL. This method will return an
// empty GURL if the `url` is nil and can return an invalid GURL.
NET_EXPORT GURL GURLWithNSURL(NSURL* url);

}  // namespace net

#endif  // NET_BASE_APPLE_URL_CONVERSIONS_H_
