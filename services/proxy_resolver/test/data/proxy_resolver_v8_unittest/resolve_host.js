// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script passes the URL's host to dnsResolveEx().
function FindProxyForURL(url, host) {
  dnsResolveEx(host);
  return 'DIRECT';
}
