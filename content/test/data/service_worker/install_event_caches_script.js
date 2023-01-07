// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', evt => {
  evt.waitUntil(async function() {
    const cache_name = 'cache_name';
    const url = '/service_worker/v8_cache_test.js'

    const cache = await caches.open(cache_name);
    const response = await fetch(url);
    await cache.put(url, response);
  }());
});
