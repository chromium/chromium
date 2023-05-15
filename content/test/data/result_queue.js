// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Queue storing asynchronous results received from the Service Worker. Results
// are sent to the test when requested.
function ResultQueue() {
  // Invariant: this.queue.length == 0 || this.pendingGets.length == 0
  this.queue = [];
  this.pendingGets = [];
}

// Adds a data item to the queue. Will be sent to the test if there are
// pendingGets.
ResultQueue.prototype.push = function(data) {
  if (this.pendingGets.length > 0) {
    const resolve = this.pendingGets.pop();
    resolve(data);
  } else {
    this.queue.unshift(data);
  }
};

// Called by native. Sends the next data item to the test if it is available.
// Otherwise increments pendingGets so it will be delivered when received.
ResultQueue.prototype.pop = function() {
  return new Promise((resolve) => {
    if (this.queue.length) {
      resolve(this.queue.pop());
    } else {
      this.pendingGets.unshift(resolve);
    }
  });
};

// Called by native. Immediately sends the next data item to the test if it is
// available, otherwise sends null.
ResultQueue.prototype.popImmediately = function() {
  return this.queue.length ? this.queue.pop() : null;
};

function formatError(error) {
  return error.name + ' - ' + error.message;
}
