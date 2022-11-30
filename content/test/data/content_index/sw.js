// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', event => {
  if (event.request.url.includes('/forcefail'))
    event.respondWith(Promise.reject());
  else if (event.request.url.includes('/forcesuccess'))
    event.respondWith(new Response(null, {status: 200}));
  else
    event.respondWith(fetch(event.request));
});