// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onMessageListener(message, sender, sendResponse) {
  switch (message) {
    case 'synchronously throw an error':
      throw new Error('synchronous error thrown');
    case 'call sendResponse synchronously':
      sendResponse('synchronous response return');
      return;
    default:
      chrome.test.fail('Unexpected test message: ' + message);
  }
}
chrome.runtime.onMessage.addListener(onMessageListener);
