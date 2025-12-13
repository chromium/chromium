// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onMessageListener(message, sender, sendResponse) {
  switch (message) {
      // JS functions are not JSON serializable. The below causes extensions v8
      // C++ logic to throw a TypeError in this context.
    case 'respond synchronously with an unserializable value':
      chrome.test.assertThrows(
          sendResponse, [() => {}], 'Could not serialize message.');
      break;
    case 'respond asynchronously with an unserializable value':
      setTimeout(() => {
        chrome.test.assertThrows(
            sendResponse, [() => {}], 'Could not serialize message.');
      }, 1);
      return true;
    default:
      chrome.test.fail('Unexpected test message: ' + message);
  }
}
chrome.runtime.onMessage.addListener(onMessageListener);
