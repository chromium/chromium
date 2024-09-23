// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', event => {
  event.respondWith(new Response('intercepted'));
});
self.addEventListener('activate', event => {
  event.waitUntil(clients.claim());
});