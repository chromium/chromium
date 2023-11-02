// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', evt => {
  evt.waitUntil(async function() {
    const c = await caches.open('bar');
    return c.addAll([
      './empty.js?1',
      './empty.js?2',
      './empty.js?3',
    ]);
  }());
});
