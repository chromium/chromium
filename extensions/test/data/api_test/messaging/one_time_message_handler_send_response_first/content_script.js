// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  // Tests that when multiple listeners are registered for runtime.onMessage and
  // the first registered responds synchronously and a later one throws an
  // error, the sender's promise resolves with the response similar to
  // github.com/mozilla/webextension-polyfill.
  async function oneTimeMessageHandlerSendResponseFirstBeforeError() {
    const response = await chrome.runtime.sendMessage('test');
    chrome.test.assertEq('response from listener', response);
    chrome.test.succeed();
  },

]);
