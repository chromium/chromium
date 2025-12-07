// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Tests that when the listener responds synchronously with a value that
  // cannot be serialized into JSON the sender is notified of the error.
  async function onMessageSyncRespondsWithUnserializableValue() {
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage(
            'respond synchronously with an unserializable value'),
        'Error: Could not serialize message.');
    chrome.test.succeed();
  },


  // Tests that when the listener responds asynchronously with a value that
  // cannot be serialized into JSON the sender is notified of the error.
  async function onMessageAsyncRespondsWithUnserializableValue() {
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendMessage(
            'respond asynchronously with an unserializable value'),
        'Error: Could not serialize message.');
    chrome.test.succeed();
  },

]);
