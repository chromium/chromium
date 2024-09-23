// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_SWITCHES_H_
#define NET_COOKIES_COOKIE_SWITCHES_H_

#include "net/base/net_export.h"

namespace net {

// This command line switch provides a means to disable partitioned cookies in
// WebView. WebView cannot disable partitioned cookies using a base::Feature
// since some apps query the cookie store before Chromium has initialized.
NET_EXPORT extern const char kDisablePartitionedCookiesSwitch[];

}  // namespace net

#endif  // NET_COOKIES_COOKIE_SWITCHES_H_
