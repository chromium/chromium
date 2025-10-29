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
  // Tests that when the listener responds synchronously with a value that
  // cannot be serialized into JSON the sender is eventually notified of the
  // error.
  async function onMessageSyncRespondsWithUnserializableValue() {
    let responsePromise = chrome.runtime.sendMessage(
        'respond synchronously with an unserializable value');
    await chrome.test.sendMessage('shutdown_worker');
    let response = await responsePromise;
    chrome.test.assertEq(undefined, response);
    chrome.test.succeed();
  },

  // Tests that when the listener responds asynchronously with a value that
  // cannot be serialized into JSON the sender is eventually notified of the
  // error.
  async function onMessageAsyncRespondsWithUnserializableValue() {
    let assertPromiseRejects = chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage(
            'respond asynchronously with an unserializable value'),
        asyncResponseFailureError);
    await chrome.test.sendMessage('shutdown_worker');
    await assertPromiseRejects;
    chrome.test.succeed();
  },

]);
