// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', e => {
  e.addRoutes([{
    condition: {
      requestMethod: "GET",
    },
    source: "fetch-event"
  }]);
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
});
