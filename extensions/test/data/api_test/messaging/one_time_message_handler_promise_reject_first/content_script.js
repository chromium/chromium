// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  // Tests that when multiple listeners are registered for runtime.onMessage and
  // the first registered listener returns a promise that rejects, and the
  // second registered listener throws a synchronous error the error is chosen
  // as the response to the sender. This is because the synchronous error will
  // always be detected first before another spin of the JS context can resolve
  // the promise. This mimics the behavior of
  // github.com/mozilla/webextension-polyfill.
  async function oneTimeMessageHandlerPromiseResolveFirst() {
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage('test'),
        'Error: Uncaught Error: sync error');
    chrome.test.succeed();
  },

]);
