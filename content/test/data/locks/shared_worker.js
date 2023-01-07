// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

onconnect = function(e) {
  var port = e.ports[0];

  port.onmessage = function(e) {
    // Acquire the lock.
    navigator.locks.request('worker_lock', {}, lock => {
      // Lock was acquired and will be released at the end of this scope.
      port.postMessage({rqid: e.data.rqid});
    });
  }

}
