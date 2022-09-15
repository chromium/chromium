// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function receiveMessage() {
  return new Promise(function(resolve) {
    navigator.serviceWorker.addEventListener('message', function(message) {
      resolve(message.data);
    });
  });
}

navigator.serviceWorker.register('register_sync_from_sw_service_worker.js')
  .then(function() {
    return receiveMessage();
  })
  .then(function(message) {
    parent.postMessage(message, '*');
  });
