// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Replies to any message with the list of URLs of all same-origin clients of
// the requested type (defaulting to 'window'), including uncontrolled ones.
self.addEventListener('message', async (event) => {
  const type =
      (event.data === 'worker' || event.data === 'all') ? event.data : 'window';
  const clients =
      await self.clients.matchAll({includeUncontrolled: true, type});
  event.source.postMessage(clients.map((client) => client.url));
});
