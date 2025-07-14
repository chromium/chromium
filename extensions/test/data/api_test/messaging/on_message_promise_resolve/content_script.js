// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: all tests cases that return a promise from the listener are validating
// that the behavior matches the behavior of
// https://github.com/mozilla/webextension-polyfill.

chrome.test.runTests([
  // Tests that a runtime.onMessage() listener can return a promise that will
  // keep the message port alive until the promise has a chance to resolve to a
  // string value.
  async function onMessagePromiseResolveStringValue() {
    const response =
        await chrome.runtime.sendMessage('return promise resolve value string');
    chrome.test.assertEq('promise resolved', response);
    chrome.test.succeed();
  },

  // Tests that a runtime.onMessage() listener can return a promise that will
  // keep the message port alive until the promise has a chance to resolve to a
  // custom object value.
  async function onMessagePromiseResolveObjectValue() {
    const response =
        await chrome.runtime.sendMessage('return promise resolve value object');
    chrome.test.assertEq({testKey: 'promise resolved'}, response);
    chrome.test.succeed();
  },

  // Tests that a runtime.onMessage() listener can return a promise that will
  // keep the message port alive until the promise has a chance to resolve to an
  // error object value. However, the Error object will not be sent along. It'll
  // be sent as an empty object.
  async function onMessagePromiseResolveErrorObjectValue() {
    const response =
        await chrome.runtime.sendMessage('return promise resolve an error');
    chrome.test.assertEq({}, response);
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

  // Tests that a runtime.onMessage() listener that synchronously responds, but
  // then returns a promise will provide the synchronous response to the caller,
  // and the promise settled value is ignored.
  async function onMessageSyncReplyAndReturnsPromise() {
    const response = await chrome.runtime.sendMessage(
        'return promise after synchronous sendResponse() is called');
    chrome.test.assertEq('synchronous response return', response);
    chrome.test.succeed();
  },

]);
