// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Replies to any message with the list of URLs of all same-origin window
// clients, including uncontrolled ones.
self.addEventListener('message', async (event) => {
  const clients =
      await self.clients.matchAll({includeUncontrolled: true, type: 'window'});
  event.source.postMessage(clients.map((client) => client.url));
});
