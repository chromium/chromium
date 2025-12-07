// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onMessageListener(message, unusedSender, sendResponse) {
  let listenerReturn = undefined;
  switch (message) {
    // JS functions are not JSON serializable. The below two cases cause
    // extensions v8 C++ logic to throw a TypeError in this context.
    case 'respond synchronously with an unserializable value':
      chrome.test.assertThrows(
          sendResponse, [() => {}], 'Could not serialize message.');
      break;
    case 'respond asynchronously with an unserializable value':
      setTimeout(() => {
        chrome.test.assertThrows(
            sendResponse, [() => {}], 'Could not serialize message.');
      }, 1);
      listenerReturn = true;
      break;
    default:
      chrome.test.fail('Unexpected test message: ' + message);
  }

  // Ensure this message is sent after the listener has returned so we know the
  // message was processed by the listener.
  setTimeout(() => {
    chrome.test.sendMessage('listener_processed_message');
  }, 1);

  return listenerReturn;
}
chrome.runtime.onMessage.addListener(onMessageListener);
