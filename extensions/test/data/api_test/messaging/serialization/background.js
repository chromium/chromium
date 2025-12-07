// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getMessageSerializationTestCases} from '/serialization_common_tests.js';

// The handlers are for the content script tests that run.

// Return the message for the test to validate.
function onMessageListener(message, unusedSender, sendResponse) {
  sendResponse(message);
}
chrome.runtime.onMessage.addListener(onMessageListener);

// Return the message for the test to validate.
chrome.runtime.onConnect.addListener(function(port) {
  port.onMessage.addListener(function(message) {
    port.postMessage(message);
  });
});

// Run background tests after receiving signal from C++ test driver.
chrome.test.sendMessage('background-script-evaluated', (message) => {
  if (message != 'start background tests') {
    return;
  }
  // Do not run the background.js tests until the C++ side indicates that the:
  //   1) tab (content script) message handlers have been registered and,
  //   2) `tabId` has been provided
  // so that the background tests will have an available tab target for their
  // messages.
  chrome.test.getConfig(function(config) {
    let tabId = parseInt(config.customArg);
    chrome.test.runTests(getMessageSerializationTestCases('tabs', tabId));
  });
});
