// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', e => {
  e.addRoutes([{
    condition: {
      urlPattern: "/service_worker/cache_corp_check",
    },
    source: {cacheName: "test"}
  }]);
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
});

self.addEventListener("fetch", e => {
  // Pass through to let the static router handle it or fallback.
});
