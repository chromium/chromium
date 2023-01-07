// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let clientList = [];

self.addEventListener('message', async e => {
  switch (e.data.command) {
    case 'StoreClients':
      e.waitUntil(new Promise(resolve => {
        done = resolve;
      }));
      clientList = await clients.matchAll({
        includeUncontrolled: true, type: 'window'});
      e.ports[0].postMessage('DONE');
      break;
    case 'PostMessageToStoredClients':
      for (const client of clientList)
        client.postMessage('hello!');
      e.ports[0].postMessage('DONE');
      done();
      break;
  }
});
