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
  e.registerRouter({
    condition: {urlPattern: "/service_worker/direct"},
    source: "network"
  });
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
});

self.addEventListener("fetch", async e => {
  e.respondWith(composeCustomResponse());
});
