// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onfetch = function(event) {
  // Redirects are resolved relative to the response's URL list. Synthetic
  // responses don't have a URL list, so a relative location header should fail
  // to redirect.
  event.respondWith(new Response('', {
    status: 302,
    headers: {location: 'foo.html'},
  }));
};
