// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  // Tests that when multiple listeners are registered for runtime.onMessage
  // and the first registered throws an error, the sender's promise reacts
  // similar to github.com/mozilla/webextension-polyfill.
  async function oneTimeMessageHandlerErrorThrownFirstError() {
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage('test'), 'Error: sync error');
    chrome.test.succeed();
  },

]);
