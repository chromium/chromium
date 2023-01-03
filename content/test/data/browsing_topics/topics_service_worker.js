// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', event => {
  event.waitUntil(async function () {
    if (!event.clientId) return;
    const client = await clients.get(event.clientId);
    if (!client) return;

    client.postMessage({
      url: event.request.url,
      topicsHeader: String(event.request.headers.get("Sec-Browsing-Topics"))
    });
  }());
});

self.addEventListener('message', event => {
  if (event.data.fetchUrl) {
    clients.matchAll().then((clients) => {
      fetch(event.data.fetchUrl, {browsingTopics: true}).then((response) => {
        // clients[0] is the most recently focused one
        clients[0].postMessage({
          finishedFetch: true
        });
      });
    });
  }
});
