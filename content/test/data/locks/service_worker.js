// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

self.onmessage = function(e) {
  console.log("received message in service worker");
  // Acquire the lock.
  navigator.locks.request('worker_lock', {}, lock => {
    // Lock was acquired and will be released at the end of this scope.
    e.ports[0].postMessage({rqid: e.data.rqid});
  });
};
