// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: all tests cases that return a promise from the listener are validating
// that the behavior matches the behavior of
// https://github.com/mozilla/webextension-polyfill.

chrome.test.runTests([

  // Tests that when the first listener returns true and the second returns a
  // promise, the faster sendResponse response is used to send the response.
  async function onMessageMultiPromiseResolve() {
    const response = await chrome.runtime.sendMessage('test');
    chrome.test.assertEq('faster sendResponse', response);
    chrome.test.succeed();
  },

]);
