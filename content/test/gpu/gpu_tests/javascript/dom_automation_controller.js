// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Script meant to be evaluated on commit by suites such as pixel and
// expected_color.

var domAutomationController = {};

domAutomationController._proceed = false;

domAutomationController._readyForActions = false;
domAutomationController._succeeded = undefined;
domAutomationController._finished = false;
domAutomationController._originalLog = window.console.log;
domAutomationController._messages = '';

domAutomationController.log = function(msg) {
  domAutomationController._messages += msg + "\n";
  domAutomationController._originalLog.apply(window.console, [msg]);
}

domAutomationController.send = function(msg) {
  domAutomationController._proceed = true;
  let lmsg = msg.toLowerCase();
  if (lmsg == "ready") {
    domAutomationController._readyForActions = true;
  } else {
    domAutomationController._finished = true;
    // Do not squelch any previous failures. Show any new ones.
    if (domAutomationController._succeeded === undefined ||
        domAutomationController._succeeded)
      domAutomationController._succeeded = (lmsg == "success");
  }
}

window.domAutomationController = domAutomationController;
