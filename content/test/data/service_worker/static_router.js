// Copyright 2023 The Chromium Authors
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
      '[ServiceWorkerStaticRouter] Response from the fetch handler',
      options);
};

self.addEventListener('install', e => {
  e.addRoutes([{
    condition: {
      urlPattern: "/service_worker/direct",
      requestMethod: "GET",
    },
    source: "network"
  }, {
    condition: {
      urlPattern: "/service_worker/direct_if_not_running",
      runningStatus: "not-running",
    },
    source: "network"
  }, {
    condition: {
      urlPattern: "/service_worker/cache_with_name",
    },
    source: {cacheName: "test"}
  }, {
    condition: {
      urlPattern: "/service_worker/cache_with_wrong_name",
    },
    source: {cacheName: "not_exist"}
  }, {
    condition: {
      urlPattern: "/service_worker/cache_*",
    },
    source: "cache"
  }, {
    condition: {
      not: {not: {urlPattern: "/service_worker/not_not_match"}}
    },
    source: "network"
  }, {
    condition: {
        urlPattern: "/service_worker/fetch_event_rule"
    },
    source: "fetch-event"
  }]);
  caches.open("test").then((c) => {
    const headers = new Headers();
    headers.append('Content-Type', 'text/html');
    headers.append('X-Response-From', 'cache');
    const options = {
      status: 200,
      statusText: 'Custom response from cache',
      headers
    };
    const response = new Response(
        '[ServiceWorkerStaticRouter] Response from the cache',
        options);
    c.put("/service_worker/cache_hit", response.clone());
    c.put("/service_worker/cache_with_name", response.clone());
    c.put("/service_worker/cache_with_wrong_name", response.clone());
  });
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
});

self.addEventListener("fetch", async e => {
  e.respondWith(composeCustomResponse());
});
