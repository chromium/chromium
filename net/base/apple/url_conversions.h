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
// valid GURL. This method will return nil if the |url| is not valid.
// Note that NSURLs should *always* be created from GURLs, so that GURL
// sanitization rules are applied everywhere.
NET_EXPORT NSURL* NSURLWithGURL(const GURL& url);

// Method for creating a valid GURL from a NSURL. This method will return an
// empty GURL if the |url| is nil.
NET_EXPORT GURL GURLWithNSURL(NSURL* url);

}  // namespace net

#endif  // NET_BASE_APPLE_URL_CONVERSIONS_H_
