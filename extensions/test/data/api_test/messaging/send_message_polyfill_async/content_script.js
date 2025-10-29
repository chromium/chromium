// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: all tests cases that return a promise from the listener are validating
// that the behavior matches the behavior of
// https://github.com/mozilla/webextension-polyfill.

const asyncResponseFailureError = `Error: A listener indicated an asynchronous \
response by returning true, but the message channel closed before a response \
was received`

chrome.test.runTests([
  // Tests that when the listener holds a reference to `sendResponse`, and
  // indicates it will reply asynchronously but then never responds, we
  // eventually get an error that it never responded.
  async function onMessageHoldSendResponseReferenceAndNeverRespond() {
    let assertPromiseRejects = chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage(
            'hold sendResponse reference but never respond'),
        asyncResponseFailureError);
    await chrome.test.sendMessage('shutdown_worker');
    await assertPromiseRejects;
    chrome.test.succeed();
  },

]);
