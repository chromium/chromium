// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

if (inIframe) {
  // Wrap a subset of GL calls in heartbeats to ensure that longer-running tests
  // still send them regularly.
  const wrappedFunctions = [
      // conformance/uniforms/no-over-optimization-on-uniform-array-*
      'getUniform',
      'getUniformLocation',
      // conformance/uniforms/uniform-samplers-test.html
      'uniform1i',
      'uniform1iv',
      // conformance2/sync/sync-webgl-specific.html
      'clientWaitSync',
      'getSyncParameter',
  ];
  for (const funcName of wrappedFunctions) {
    wrapFunctionInHeartbeat(WebGLRenderingContext.prototype, funcName);
    wrapFunctionInHeartbeat(WebGL2RenderingContext.prototype, funcName);
  }
}

if (!inIframe) {
  var testHarness = {};

  testHarness.reset = function() {
    testHarness._allTestSucceeded = true;
    testHarness._messages = '';
    testHarness._failures = 0;
    testHarness._totalTests = 0;
    testHarness._finished = false;
  }
  testHarness.reset();

  testHarness._originalLog = window.console.log;

  testHarness.log = function(msg) {
    wrapper.sendHeartbeatThrottled();
    testHarness._messages += msg + "\n";
    testHarness._originalLog.apply(window.console, [msg]);
  }

  testHarness.reportResults = function(url, success, msg) {
    wrapper.sendHeartbeatThrottled();
    testHarness._allTestSucceeded = testHarness._allTestSucceeded && !!success;
    testHarness._totalTests++;
    if(!success) {
      testHarness._failures++;
      if(msg) {
        testHarness.log(msg);
      }
    }
  };

  testHarness.notifyFinished = function(url) {
    wrapper.sendTestFinished();
    testHarness._finished = true;
  };

  testHarness.navigateToPage = function(src) {
    wrapper.sendHeartbeatThrottled();
    var testFrame = document.getElementById("test-frame");
    testFrame.src = src;
  };
} else {
  var testHarness = window.parent.testHarness;
}

window.webglTestHarness = testHarness;
window.console.log = testHarness.log;
window.onerror = function(message, url, line) {
  testHarness.reportResults(null, false, message);
  testHarness.notifyFinished(null);
};
window.quietMode = function() {
  wrapper.sendHeartbeatThrottled();
  return true;
}