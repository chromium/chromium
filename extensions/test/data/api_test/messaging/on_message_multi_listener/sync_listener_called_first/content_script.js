// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  // Tests that an sync listener registering first will still provide it's
  // response to the message sender.
  async function sendMessageWithSyncListenerCalledFirstPromise() {
    const response = await chrome.runtime.sendMessage('');
    chrome.test.assertEq('Async response from background script', response);
    chrome.test.succeed();
  },

]);
