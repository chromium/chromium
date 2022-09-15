// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', event => {
  const url = new URL(event.request.url);
  if (url.search != '?dump_headers')
    return;

  event.respondWith((async () => {
    const result = {headers: []};
    event.request.headers.forEach((value, name) => {
      result.headers.push([name, value]);
    });
    return new Response(
        JSON.stringify(result), {headers: [['content-type', 'text/html']]});
  })());
});
