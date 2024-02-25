// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared JavaScript code related to the heartbeat mechanism used by several
// GPU tests. Intended to be evaluated on commit in addition to whatever
// suite-specific code there is.

const HEARTBEAT_THROTTLE_MS = 5000;

class WebSocketWrapper {
  constructor() {
    this.queued_messages = [];
    this.throttle_timer = null;
    this.last_heartbeat = null;
    this.socket = null;

    this._sendDelayedHeartbeat = this._sendDelayedHeartbeat.bind(this);
  }

  setWebSocket(s) {
    this.socket = s;
    s.send('{"type": "CONNECTION_ACK"}');
    for (let qm of this.queued_messages) {
      s.send(qm);
    }
  }

  _sendMessage(message) {
    if (this.socket === null) {
      this.queued_messages.push(message);
    } else {
      this.socket.send(message);
    }
  }

  _sendHeartbeat() {
    this._sendMessage('{"type": "TEST_HEARTBEAT"}');
  }

  _sendDelayedHeartbeat() {
    this.throttle_timer = null;
    this.last_heartbeat = +new Date();
    this._sendHeartbeat();
  }

  sendHeartbeatThrottled() {
    const now = +new Date();
    // Heartbeat already scheduled.
    if (this.throttle_timer !== null) {
      // If we've already passed the point in time where the heartbeat should
      // have been sent, cancel it and send it immediately. This helps in cases
      // where we've scheduled one, but the test is doing so much work that
      // the callback doesn't fire in a reasonable amount of time.
      if (this.last_heartbeat !== null &&
          now - this.last_heartbeat >= HEARTBEAT_THROTTLE_MS) {
        this._clearPendingHeartbeat();
        this.last_heartbeat = now;
        this._sendHeartbeat();
      }
      return;
    }

    // Send a heartbeat immediately.
    if (this.last_heartbeat === null ||
        now - this.last_heartbeat >= HEARTBEAT_THROTTLE_MS){
      this.last_heartbeat = now;
      this._sendHeartbeat();
      return;
    }
    // Schedule a heartbeat for the future.
    this.throttle_timer = setTimeout(
        this._sendDelayedHeartbeat,
        HEARTBEAT_THROTTLE_MS - (now - this.last_heartbeat));
  }

  _clearPendingHeartbeat() {
    if (this.throttle_timer !== null) {
      clearTimeout(this.throttle_timer);
      this.throttle_timer = null;
    }
  }

  sendTestStarted() {
    this._sendMessage('{"type": "TEST_STARTED"}');
  }

  sendTestFinished() {
    this._clearPendingHeartbeat();
    this._sendMessage('{"type": "TEST_FINISHED"}');
  }

  // Pixel test messages.
  sendTestFinishedWithSuccessValue(success) {
    this._clearPendingHeartbeat();
    this._sendMessage(`{"type": "TEST_FINISHED", "success": ${success}}`);
  }

  sendPerformPageAction() {
    this._sendMessage('{"type": "PERFORM_PAGE_ACTION"}');
  }

  sendTestContinue() {
    this._sendMessage('{"type": "TEST_CONTINUE"}')
  }
}

if (window.parent.wrapper !== undefined) {
  var wrapper = window.parent.wrapper;
  var inIframe = true;
  window.wrapper = window.parent.wrapper;
} else {
  var wrapper = new WebSocketWrapper();
  var inIframe = false;
  window.wrapper = wrapper;
}

function connectWebsocket(port) {
  let socket = new WebSocket('ws://127.0.0.1:' + port);
  socket.addEventListener('open', () => {
    wrapper.setWebSocket(socket);
  });
}

function wrapFunctionInHeartbeat(prototype, key) {
  const old = prototype[key];
  // Some functions are specific to a WebGL version, so don't try to wrap
  // functions that don't exist in the current version's context prototype.
  if (old === undefined) {
    return;
  }
  prototype[key] = function (...args) {
    wrapper.sendHeartbeatThrottled();
    return old.call(this, ...args);
  }
}
