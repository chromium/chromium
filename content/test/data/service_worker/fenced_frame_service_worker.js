// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', e => e.waitUntil(skipWaiting()));
self.addEventListener('activate', e => e.waitUntil(clients.claim()));

self.addEventListener('message', async e => {
  const {type, id, resource, options} = e.data;
  if (type === 'fetch') {
    try {
      await self.registration.backgroundFetch.fetch(id, resource, options);
      e.source.postMessage('fetch succeeded.');
    } catch (error) {
      e.source.postMessage(error.message);
    }
  }
});
