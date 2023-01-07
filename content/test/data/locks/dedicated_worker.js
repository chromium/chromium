// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

self.onmessage = function(e) {
  // Acquire the lock.
  navigator.locks.request('worker_lock', {}, lock => {
    // Lock was acquired and will be released at the end of this scope.
    self.postMessage({rqid: e.data.rqid});
  });
};
