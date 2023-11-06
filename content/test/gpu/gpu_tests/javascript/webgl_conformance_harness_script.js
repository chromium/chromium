// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Script meant to be evaluated on commit by WebGL tests. Requires that
// websocket_heartbeat.js be prepended or otherwise evaluated first.

if (inIframe) {
  // Wrap a subset of GL calls in heartbeats to ensure that longer-running tests
  // still send them regularly.
  const wrappedGLFunctions = [
      'getError',
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
  for (const funcName of wrappedGLFunctions) {
    wrapFunctionInHeartbeat(WebGLRenderingContext.prototype, funcName);
    wrapFunctionInHeartbeat(WebGL2RenderingContext.prototype, funcName);
  }

  // Do the same for HTML canvas elements.
  const wrappedHTMLCanvasFunctions = [
      'getContext',
  ];
  for (const funcName of wrappedHTMLCanvasFunctions) {
    wrapFunctionInHeartbeat(HTMLCanvasElement.prototype, funcName);
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