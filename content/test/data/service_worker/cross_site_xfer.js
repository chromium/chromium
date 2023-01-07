// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onfetch = function(event) {
  if (event.request.url.indexOf(
        'cross_site_xfer_confirm_via_serviceworker.html') != -1) {
    event.respondWith(fetch('cross_site_xfer_confirm.html'));
    return;
  }
  if (event.request.url.indexOf('cross_site_xfer_subresource') != -1) {
    event.respondWith(new Response(new Blob(['Hello'])));
    return;
  }
};
