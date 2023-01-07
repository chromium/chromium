// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function postMessageToWindow(msg) {
  for (const client of await clients.matchAll({includeUncontrolled: true}))
    client.postMessage(msg);
}

async function reregisterContent() {
  try {
    await self.registration.index.add({
      id: 'new id',
      title: 'same title',
      description: 'same description',
      category: 'article',
      icons: [{
        src: '/resources/square.png',
      }],
      url: 'resources/',
    });
    await postMessageToWindow('Successfully registered');
  } catch (e) {
    await postMessageToWindow(e.message);
  }
}

self.addEventListener('contentdelete', event => {
  if (event.id === 'register-again') {
    event.waitUntil(reregisterContent());
    return;
  }

  event.waitUntil(postMessageToWindow(event.id));
});
