// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  // Tests that when multiple `async functions` (which always return promises)
  // are added as listeners, the faster listener's promise to resolve (even if
  // it is not the first listener registered) is the promise that is used as the
  // message response.
  async function onMessageMultiAsyncFunctionPromiseResolve() {
    const response = await chrome.runtime.sendMessage('test');
    chrome.test.assertEq('faster async function (promise) response', response);
    chrome.test.succeed();
  },

]);
