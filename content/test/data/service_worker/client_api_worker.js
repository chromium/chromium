// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('message', async (event) => {
  if (event.data.command == 'navigate') {
    const url = event.data.url;
    try {
      await event.source.navigate(url);
    } catch (err) {
      event.source.postMessage('navigate failed');
    }
  }
});

self.addEventListener('notificationclick', event => {
  clients.openWindow(event.notification.body);
});

