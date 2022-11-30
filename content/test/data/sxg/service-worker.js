// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('activate', (e) => {
  if (location.href.endsWith('?navigation-preload')) {
    e.waitUntil(self.registration.navigationPreload.enable())
  }
})

self.addEventListener('fetch', (event) => {
  if (location.href.endsWith('?generated')) {
    event.respondWith(new Response(
      '<script>\n' +
      'window.parent.document.title =\'' +
      event.request.headers.get('accept') + '\';\n</script>',
      {headers:[['content-type', 'text/html']]}));
  } else if (location.href.endsWith('?navigation-preload')) {
    event.respondWith(event.preloadResponse);
  }
});
