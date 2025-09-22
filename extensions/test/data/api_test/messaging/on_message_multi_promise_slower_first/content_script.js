// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: all tests cases that return a promise from the listener are validating
// that the behavior matches the behavior of
// https://github.com/mozilla/webextension-polyfill.

chrome.test.runTests([

  // Tests that when multiple listeners return promises, the sender receives a
  // response from the first promise to resolve. Even if the faster promise is
  // registered second.
  async function onMessageMultiPromiseResolve() {
    const response = await chrome.runtime.sendMessage('test');
    chrome.test.assertEq('faster promise', response);
    chrome.test.succeed();
  },

]);
