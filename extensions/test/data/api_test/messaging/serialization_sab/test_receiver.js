// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The receiver listens for incoming messages. It expects a validation ping
// after invalid messages are sent and dropped. If any unexpected message is
// received , it fails the test.
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  // Ignore messages meant for the background script.
  if (message && message.testResult) {
    return;
  }
  if (message === 'ping') {
    sendResponse('pong');
    return;
  }
  chrome.test.fail(`The receiver should not receive an unexpected message: ${
      JSON.stringify(message)})`);
});

// Notify background that we are ready to receive messages.
chrome.runtime.sendMessage({receiverReady: true});
