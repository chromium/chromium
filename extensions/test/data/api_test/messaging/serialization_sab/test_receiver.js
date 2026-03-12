// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener((message) => {
  // Ignore messages meant for the background script.
  if (message && message.testResult) {
    return;
  }
  chrome.test.fail('The receiver should not receive a message.');
});

// Notify background that we are ready to receive messages.
chrome.runtime.sendMessage({receiverReady: true});
