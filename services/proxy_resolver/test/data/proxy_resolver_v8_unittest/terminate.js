// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function FindProxyForURL(url, host) {
  if (host != 'hang')
    return 'PROXY ' + host + ':88';

  var ip = dnsResolve("host1");

  // The following may or may not be executed, even if dnsResolve() terminates
  // the script execution.
  dnsResolveEx("host2");
  dnsResolveEx("host3");
  alert("hahaha");

  // Hang!
  for (;;) {}

  // The following definitely won't be executed, since control should never
  // make it past the preceding hang.
  dnsResolve("host4");
  dnsResolve("host5");
  alert("uhm...");
  throw "not reached";
}
