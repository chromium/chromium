// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onmessage = e => {
  // Collect all client URLs in this origin.
  const options = { includeUncontrolled: true, type: 'all' };
  const promise = self.clients.matchAll(options)
      .then(clients => {
        const client_urls = [];
        clients.forEach(client => client_urls.push(client.url));
        e.source.postMessage(client_urls);
      });
  e.waitUntil(promise);
};
