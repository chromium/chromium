// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function sendMessageToClients(message) {
  clients.matchAll({ includeUncontrolled: true }).then(function(clients) {
    clients.forEach(function(client) {
      client.postMessage(message);
    });
  });
}

self.addEventListener('activate', function(event) {
  registration.sync.register('foo')
    .then(function () {
      sendMessageToClients('registration succeeded');
    }, function(e) {
      sendMessageToClients('registration failed');
    });
});
