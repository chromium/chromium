// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(async (config) => {
  let polyfillSupportEnabled = config.customArg === 'true';

  chrome.test.runTests([

    // Tests that when multiple listeners are registered for runtime.onMessage
    // and the first registered throws an error, the sender's promise reacts
    // similar to github.com/mozilla/webextension-polyfill (if
    // polyfillSupportEnabled is `true`), and vice versa.
    async function oneTimeMessageHandlerErrorThrownFirstError() {
      if (polyfillSupportEnabled) {
        await chrome.test.assertPromiseRejects(
            chrome.runtime.sendMessage('test'),
            'Error: Uncaught Error: sync error');
        chrome.test.succeed();
      } else {
        const response = await chrome.runtime.sendMessage('test');
        chrome.test.assertEq('response from listener', response);
        chrome.test.succeed();
      }
    },

  ]);
});
