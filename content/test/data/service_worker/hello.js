// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', (event) => {
  // Responds to requests containing "hello_sw".
  if (event.request.url.includes('hello_sw')) {
    event.respondWith(
      fetch('hello-from-sw.txt').then((response) => {
        return response;
      }));
  }
});

self.addEventListener('message', (event) => {
  if (event.data == 'postMessage from the page') {
    event.source.postMessage('postMessage from the service worker');
  }
});
