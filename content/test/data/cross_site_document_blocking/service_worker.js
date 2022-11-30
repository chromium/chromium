// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This service worker is used by the CrossSiteDocumentBlockingServiceWorkerTest
// browser test - please see the comments there for more details.

function createHtmlNoSniffResponse() {
  var headers = new Headers();
  headers.append('Content-Type', 'text/html');
  headers.append('X-Content-Type-Options', 'nosniff');
  return new Response('Response created by service worker',
                      { status: 200, headers: headers });
}

self.addEventListener('fetch', function(event) {
  // This handles response to the request issued in the
  // CrossSiteDocumentBlockingServiceWorkerTest.NoNetwork test.
  if (event.request.url.endsWith('data_from_service_worker')) {
    event.respondWith(createHtmlNoSniffResponse());
    return;
  }

  // This handles response to the request issued in the
  // CrossSiteDocumentBlockingServiceWorkerTest.NetworkToServiceWorkerResponse
  // test.
  if (event.request.url.endsWith('nosniff.txt')) {
    event.respondWith(
        fetch(event.request).then(
            response => Promise.reject('Expected error from service worker'),
            error => Promise.reject('Unexpected error: ' + error)));
    return;
  }

  // Let the request go to the network in all the other cases (e.g. when
  // reloading the test page at cross_site_document_blocking/request.html).
});
