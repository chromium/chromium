// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Tests that a runtime.onMessage() listener can return a promise that will
  // keep the message port alive until the promise has a chance to resolve.
  async function onMessagePromiseResolve() {
    const response =
        await chrome.runtime.sendMessage('return promise resolve value');
    chrome.test.assertEq('promise resolved', response);
    chrome.test.succeed();
  },

  // Tests that a runtime.onMessage() listener can return an async function
  // (e.g. a promise) that will keep the message port alive until the promise
  // has a chance to settle.
  async function onMessageAsyncFunction() {
    const response =
        await chrome.runtime.sendMessage('return promise as an async function');
    chrome.test.assertEq('async function promise return', response);
    chrome.test.succeed();
  },

  // Tests that a runtime.onMessage() listener that returns a promise that
  // contains an asynchronous call to sendResponse() will return the result of
  // the promise, not the value passed to sendResponse() to the the sender's
  // response callback.
  async function onMessagePromiseAsyncReply() {
    const response = await chrome.runtime.sendMessage(
        'call sendResponse asynchronously after returned promise resolves');
    chrome.test.assertEq('outer promise return', response);
    chrome.test.succeed();
  },

  // Tests that a runtime.onMessage() listener that synchronously calls
  // sendResponse() causes the the sender's response callback to be provided the
  // synchronous response call's value.
  async function onMessageSyncReply() {
    const response =
        await chrome.runtime.sendMessage('call sendResponse synchronously');
    chrome.test.assertEq('synchronous response return', response);
    chrome.test.succeed();
  },

  // Tests that a runtime.onMessage() listener that synchronously responds, but
  // then returns a promise will provide the synchronous response to the caller,
  // and the promise settled value is ignored.
  async function onMessageSyncReplyAndReturnsPromise() {
    const response = await chrome.runtime.sendMessage(
        'return promise after synchronous sendResponse() is called');
    chrome.test.assertEq('synchronous response return', response);
    chrome.test.succeed();
  },

  // Tests that a runtime.onMessage() listener that throws an error
  // synchronously returns an `undefined` response.
  async function onMessageSyncThrowsError() {
    const response =
        await chrome.runtime.sendMessage('synchronously throw an error');
    chrome.test.assertEq(undefined, response);
    chrome.test.succeed();
  },

]);
