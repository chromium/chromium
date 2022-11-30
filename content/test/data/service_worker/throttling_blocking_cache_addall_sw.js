// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', evt => {
  evt.waitUntil(async function() {
    const c = await caches.open('foo');
    return c.addAll([
      './empty.js?1&block',
      './empty.js?2&block',
      './empty.js?3&block',
    ]);
  }());
});
