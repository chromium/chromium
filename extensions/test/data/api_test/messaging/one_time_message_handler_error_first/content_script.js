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
        const response = await chrome.runtime.sendMessage('test');
        chrome.test.assertEq('response from listener', response);
        chrome.test.succeed();
        // TODO(crbug.com/439644930): The below is the current polyfill
        // behavior. The polyfill grabs the first error and returns that to the
        // listener because it's injected into the JS, but because we execute
        // the listeners first (EventEmitter::DispatchSync()), then later check
        // their results (OneTimeMessageHandler::OnEventFired() the
        // sendResponse() in this example is able to finish first. We currently
        // rely on this behavior to prevent leaving message ports open longer
        // than they need to, so it might add significant complexity to support.
        // Let's confirm this is the ultimate desired behavior before
        // implementing.
        // await chrome.test.assertPromiseRejects(
        //     chrome.runtime.sendMessage('test'),
        //     'Error: Uncaught Error: sync error');
        // chrome.test.succeed();
      } else {
        const response = await chrome.runtime.sendMessage('test');
        chrome.test.assertEq('response from listener', response);
        chrome.test.succeed();
      }
    },

  ]);
});
