// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
const OFFLINE_MAIN_RESOURCE_URL =
    'race_network_request_from_fetch_handler.html';
const OFFLINE_SUB_RESOURCE_URL = 'hello-from-sw.txt';
const CACHE_NAME = 'race_network_request';

self.addEventListener('install', e => {
  const modifyHeader = response => {
    const customHeaders = new Headers(response.headers);
    customHeaders.append('X-Response-From', 'fetch-handler');
    return new Response(response.body, {
      status: response.status,
      statusText: response.statusText,
      headers: customHeaders
    });
  };

  self.skipWaiting();
  e.waitUntil((async () => {
    const cache = await caches.open(CACHE_NAME);
    const main_resource_response = await fetch(OFFLINE_MAIN_RESOURCE_URL);
    const sub_resource_response = await fetch(OFFLINE_SUB_RESOURCE_URL);
    await cache.put(
        OFFLINE_MAIN_RESOURCE_URL, modifyHeader(main_resource_response));
    await cache.put(
        OFFLINE_SUB_RESOURCE_URL, modifyHeader(sub_resource_response));
  })());
});

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
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
  if (url.search.includes('timeout')) {
    timeout = new Promise(resolve => setTimeout(resolve, 1500));
  }

  // Force respond from the cache
  if (url.search.includes('respond_from_fetch_handler')) {
    const is_navigation_request = request.mode == 'navigate';
    e.respondWith(
      (async () => {
        await timeout;
        const cache = await caches.open(CACHE_NAME);
        return await cache.match(
            is_navigation_request ? OFFLINE_MAIN_RESOURCE_URL :
                                    OFFLINE_SUB_RESOURCE_URL);
      })()
    );
  }
});
