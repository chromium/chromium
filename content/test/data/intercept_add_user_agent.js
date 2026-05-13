// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('activate', e => {
  e.waitUntil(self.registration.navigationPreload.enable());
  e.waitUntil(clients.claim());
});

self.addEventListener('fetch', e => {
  e.respondWith(fetch(e.request.url, {headers: {'User-Agent': 'foo'}}));
});
