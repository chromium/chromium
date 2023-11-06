// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Script meant to be evaluated on commit by suites such as pixel and
// expected_color. Requires that websocket_heartbeat.js be prepended or
// otherwise evaluated first.

var test_iframe = undefined;
function getIframe() {
  if (test_iframe === undefined) {
    test_iframe = document.getElementById('test_iframe');
  }
  return test_iframe
}

function evalInIframe(statement) {
  return getIframe().contentWindow.eval(statement);
}

if (inIframe) {
  const wrappedFunctions = [
      // These were originally copied from the WebGL conformance tests since
      // they implemented a similar heartbeat mechanism first, so some of these
      // are likely unnecessary. However, the tests are happy, so they are
      // kept as-is for now.
      'getUniform',
      'getUniformLocation',
      'uniform1i',
      'uniform1iv',
      'clientWaitSync',
      'getSyncParameter',
  ];
  for (const funcName of wrappedFunctions) {
    wrapFunctionInHeartbeat(WebGLRenderingContext.prototype, funcName);
    wrapFunctionInHeartbeat(WebGL2RenderingContext.prototype, funcName);
  }
}

if (!inIframe) {
  var domAutomationController = {};

  domAutomationController._originalLog = window.console.log;
  domAutomationController._messages = '';

  domAutomationController.log = function(msg) {
    domAutomationController._messages += msg + "\n";
    domAutomationController._originalLog.apply(window.console, [msg]);
  }

  domAutomationController.send = function(msg) {
    let lmsg = msg.toLowerCase();
    if (lmsg == "ready") {
      wrapper.sendPerformPageAction();
    } else if (lmsg == "continue") {
      wrapper.sendTestContinue();
    } else {
      wrapper.sendTestFinishedWithSuccessValue(lmsg == "success");
    }
  }

  window.domAutomationController = domAutomationController;
} else {
  var domAutomationController = window.parent.domAutomationController;
}
