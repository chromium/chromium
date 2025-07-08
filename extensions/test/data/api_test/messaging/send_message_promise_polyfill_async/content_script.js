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
  // Tests that when the listener holds a reference to sendResponse, and
  // indicates it will reply asynchronously, but then never responds we
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

  // Tests that when the listener responds synchronously with a value that
  // cannot be serialized into JSON the sender is eventually notified of the
  // error.
  // TODO(crbug.com/40753031): Move this test to the synchronous test version
  // once it's fixed and responds synchronously.
  async function onMessageSyncRespondsWithUnserializableValue() {
    let responsePromise = chrome.runtime.sendMessage(
        'respond synchronously with an unserializable value');
    await chrome.test.sendMessage('shutdown_worker');
    let response = await responsePromise;
    chrome.test.assertEq(undefined, response);
    // TODO(crbug.com/40753031): This is the future desired behavior.
    // await chrome.test.assertPromiseRejects(
    //     chrome.runtime.sendMessage(
    //         'respond synchronously with an unserializable value'),
    //     asyncResponseFailureError);
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
