// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
const composeCustomResponse = () => {
  const headers = new Headers();
  headers.append('Content-Type', 'text/html');
  headers.append('X-Response-From', 'fetch-handler');
  const options = {
    status: 200,
    statusText: 'Custom response from fetch handler',
    headers
  };

  return new Response(
      '[ServiceWorkerRaceNetworkRequest] Response from the fetch handler',
      options);
};

const errors = [];

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
});

self.addEventListener('fetch', e => {
  const {request} = e;
  const url = new URL(request.url);

  // Force slow response
  if (url.search.includes('sw_slow')) {
    const start = Date.now();
    while (true) {
      if (Date.now() - start > 1500) {
        break;
      }
    }
  }

  // Force fallback
  if (url.search.includes('sw_fallback')) {
    return;
  }

  // Force respond from the cache
  if (url.search.includes('sw_respond')) {
    e.respondWith(composeCustomResponse());
  }

  if (url.search.includes('sw_pass_through')) {
    e.respondWith(fetch(request).catch(err => {
      errors.push(err);
    }));
  }

  if (url.search.includes('sw_clone_pass_through')) {
    e.respondWith(fetch(request.clone()).catch(err => {
      errors.push(err);
    }));
  }
});

self.addEventListener('message', e => {
  if (e.data == 'errors') {
    e.source.postMessage(errors);
  }
});
