// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onMessageListener(unusedMessage, unusedSender, sendResponse) {
  self.holdSendResponse = sendResponse;

  // Ensure this message is sent after the listener has returned so we know the
  // message was processed by the listener.
  setTimeout(() => {
    chrome.test.sendMessage('listener_processed_message');
  }, 1);

  return true;
}
chrome.runtime.onMessage.addListener(onMessageListener);
