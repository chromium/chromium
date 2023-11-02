// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('activate', event => {
  event.waitUntil(self.registration.navigationPreload.enable());
});

self.addEventListener('fetch', event => {
  const params = new URL(event.request.url).searchParams;

  if (event.request.mode == 'navigate') {
    if (params.has('navpreload_or_offline')) {
      event.respondWith((async () => {
        try {
          return await event.preloadResponse;
        } catch (e) {
          return new Response("Hello Offline page");
        }
      })());
    } else {
      event.respondWith(event.preloadResponse);
    }
  }
});
