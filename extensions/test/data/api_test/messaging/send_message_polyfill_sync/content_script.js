// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: all tests cases that return a promise from the listener are validating
// that the behavior matches the behavior of
// https://github.com/mozilla/webextension-polyfill.

chrome.test.runTests([
  // Tests that a runtime.onMessage() listener that synchronously calls
  // sendResponse() causes the the sender's promise to be resolved with the
  // synchronous response call's value.
  async function onMessageSyncReply() {
    const response =
        await chrome.runtime.sendMessage('call sendResponse synchronously');
    chrome.test.assertEq('synchronous response return', response);
    chrome.test.succeed();
  },

]);
