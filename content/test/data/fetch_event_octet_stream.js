// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', e => {
  const headers = new Headers();
  const filename = 'octet-stream.abc';
  headers.append('Content-Type', 'application/octet-stream; charset=UTF-8');
  headers.append('Content-Disposition',
                 'attachment; filename="' + filename +
                 '"; filename*=UTF-8\'\'' + filename);
  e.respondWith(new Response('This is a binary', {headers}));
});
