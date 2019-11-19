// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', evt => {
  evt.waitUntil(async function() {
    const cache_name = 'cache_name';

    const url = new URLSearchParams(self.location.search).get('script_url');

    const cache = await caches.open(cache_name);
    const response = await fetch(url, { mode: 'no-cors' });
    await cache.put(url, response);
  }());
});
