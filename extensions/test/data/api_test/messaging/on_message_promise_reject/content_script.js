// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: all tests cases that return a promise from the listener are validating
// that the behavior matches the behavior of
// https://github.com/mozilla/webextension-polyfill.

// Test suite that the message port will stay alive until a promise returned
// from an runtime.onMessage() listener has a chance to reject.
chrome.test.runTests([
  // Tests if the rejection value is an Error object, a new error object will be
  // returned with the original Error's .message property to the caller.
  async function onMessagePromiseRejectWithErrorObject() {
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage('return promise reject with error object'),
        'Error: Uncaught Error: promise rejected error message');
    chrome.test.succeed();
  },

  // Tests if the rejection value is a non-Error object with a string .message
  // property, a new Error object will be returned with a generic error message.
  // Any custom properties will not be passed along.
  async function onMessagePromiseRejectWithCustomObjectMessage() {
    try {
      await chrome.runtime.sendMessage(
          'return promise reject with custom object that has message key');
      chrome.test.fail('the promise was resolved, not rejected as expected');
    } catch (error) {
      chrome.test.assertEq(
          'A runtime.onMessage listener\'s promise rejected without an Error',
          error.message);
      // Only the .message property from the listener's rejected promise
      // will be provided to the message sender.
      chrome.test.assertFalse(error.hasOwnProperty('testKey'));
      chrome.test.succeed();
    }
  },

  // Tests if the rejection value is not an Error, but is an object (without a
  // .message property) then a generic Error will be provided to the caller.
  async function onMessagePromiseRejectWithNonErrorObject() {
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage(
            'return promise reject with a non-error object'),
        'Error: A runtime.onMessage listener\'s promise rejected without an ' +
            'Error');
    chrome.test.succeed();
  },

  // Tests if the rejection value is not an object then a generic Error will be
  // provided to the caller.
  async function onMessagePromiseRejectWithNonObject() {
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage(
          'return promise reject with a non-object'),
        'Error: A runtime.onMessage listener\'s promise rejected without an ' +
            'Error');
    chrome.test.succeed();
  },

  // Tests if the rejection value is not passed anything, then a generic Error
  // will be provided to the caller.
  async function onMessagePromiseRejectWithNoRejectValue() {
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage(
            'return promise reject with no reject value'),
        'Error: A runtime.onMessage listener\'s promise rejected without an ' +
            'Error');
    chrome.test.succeed();
  },

  async function onMessagePromiseRejectWithUndefinedValue() {
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage('return promise reject with undefined'),
        'Error: A runtime.onMessage listener\'s promise rejected without an ' +
            'Error');
    chrome.test.succeed();
  },

  // TODO(crbug.com/424560420): Also test sender callback behavior for promise
  // returns. mozilla/webextension-polyfill doesn't support callbacks but
  // Firefox does so it'd be good to know what our current behavior is for
  // future reference.
]);
