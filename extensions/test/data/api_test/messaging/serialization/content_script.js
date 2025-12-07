// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This works-around that content scripts can't import because they aren't
// considered modules.
(async () => {
  const src = chrome.runtime.getURL('/serialization_common_tests.js');
  const testsImport = await import(src);
  chrome.test.runTests(testsImport.getMessageSerializationTestCases('runtime'));
})();

// The handlers below are for the background tests that run.

// Return the message for the test to validate.
function onMessageListener(message, unusedSender, sendResponse) {
  sendResponse(message);
}
chrome.runtime.onMessage.addListener(onMessageListener);

// Return the message for the test to validate.
chrome.runtime.onConnect.addListener(function(port) {
  port.onMessage.addListener(function(msg) {
    port.postMessage(msg);
  });
});

// This ensures the content script message handlers have registered and are
// ready to receive events for the background script tests.
setTimeout(chrome.test.sendMessage, 1, 'content-message-handlers-registered');
