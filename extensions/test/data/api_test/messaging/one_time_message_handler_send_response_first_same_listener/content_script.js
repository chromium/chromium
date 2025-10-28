// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  // Tests that when a single listener is registered for runtime.onMessage and
  // it sends a response and then throws an error the sender's promise resolves
  // with the response similar to github.com/mozilla/webextension-polyfill.
  async function oneTimeMessageHandlerListenerRespondsBeforeError() {
    const response = await chrome.runtime.sendMessage('test');
    chrome.test.assertEq('response from listener', response);
    chrome.test.succeed();
  },

]);
