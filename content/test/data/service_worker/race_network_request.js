// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
const OFFLINE_URL = 'race_network_request_from_fetch_handler.html'
const CACHE_NAME = 'race_network_request';

self.addEventListener('install', e => {
  e.waitUntil(
    (async () => {
      const cache = await caches.open(CACHE_NAME);
      const response = await fetch(OFFLINE_URL);

      const customHeaders = new Headers(response.headers);
      customHeaders.append('X-Response-From', 'fetch-handler');

      await cache.put(OFFLINE_URL, new Response(response.body, {
                        status: response.status,
                        statusText: response.statusText,
                        headers: customHeaders
                      }));
    })()
  );
  self.skipWaiting();
});

self.addEventListener("activate", () => {
  self.clients.claim();
});

self.addEventListener("fetch", async e => {
  const {request} = e;
  const url = new URL(request.url);

  // Force fallback
  if (url.search.includes('fallback')) {
    return;
  }

  // Force timeout
  let timeout = Promise.resolve();
  if (request.mode == 'navigate' && url.search.includes('timeout')) {
    timeout = new Promise(resolve => setTimeout(resolve, 1500));
  }

  // Force respond from the cache
  if (url.search.includes('respond_from_fetch_handler')) {
    e.respondWith(
      (async () => {
        await timeout;
        const cache = await caches.open(CACHE_NAME);
        return await cache.match(OFFLINE_URL);
      })()
    );
  }
});
