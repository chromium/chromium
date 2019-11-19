// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_URL_VERIFICATION_CONSTANTS_H_
#define IOS_WEB_PUBLIC_DEPRECATED_URL_VERIFICATION_CONSTANTS_H_

namespace web {
enum URLVerificationTrustLevel {
  // The returned URL cannot be trusted.
  kNone,
  // The returned URL can be displayed to the user, but the DOM of the
  // UIWebView doesn't match yet.
  kMixed,
  // The returned URL can be trusted.
  kAbsolute,
};
}  // namespace web

#endif  // IOS_WEB_PUBLIC_DEPRECATED_URL_VERIFICATION_CONSTANTS_H_
