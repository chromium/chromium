// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g_iteration = 0;

function FindProxyForURL(url, host) {
  g_iteration++;

  var ip1 = dnsResolve("host1");
  var ip2 = dnsResolveEx("host2");

  if (ip1 == "182.111.0.222" && ip2 == "111.33.44.55")
    return "PROXY foopy:" + g_iteration;

  // If the script didn't terminate when abandoned, then it will reach this and
  // hang.
  for (;;) {}
  throw "not reached";
}
