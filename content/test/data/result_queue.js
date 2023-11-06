// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Queue storing asynchronous results. Results are available via `pop` when
// requested.
function ResultQueue() {
  // Invariant: this.queue.length == 0 || this.pendingGets.length == 0
  this.queue = [];
  this.pendingGets = [];
}

// Adds a data item to the queue or sends it to the earliest pending `pop`
// operation.
ResultQueue.prototype.push = function(data) {
  if (this.pendingGets.length > 0) {
    const resolve = this.pendingGets.pop();
    resolve(data);
  } else {
    this.queue.unshift(data);
  }
};

// Returns a promise that resolves with the next data item, when available.
ResultQueue.prototype.pop = function() {
  return new Promise((resolve) => {
    if (this.queue.length) {
      resolve(this.queue.pop());
    } else {
      this.pendingGets.unshift(resolve);
    }
  });
};

// Immediately returns the next data item if it is available, otherwise returns
// null.
ResultQueue.prototype.popImmediately = function() {
  return this.queue.length ? this.queue.pop() : null;
};
