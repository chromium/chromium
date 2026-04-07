// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  chrome.test.assertTrue(self.crossOriginIsolated);
  chrome.test.assertFalse(typeof SharedArrayBuffer === 'undefined');

  const urlParams = new URLSearchParams(window.location.search);
  const receiverId = parseInt(urlParams.get('receiverId'));
  chrome.test.assertFalse(isNaN(receiverId));

  // Test sending a raw `SharedArrayBuffer`. This should fail synchronously
  // on the sender side during structured clone message serialization.
  const sab = new SharedArrayBuffer(16);
  const view = new Int32Array(sab);
  view[0] = 1337;

  try {
    await chrome.tabs.sendMessage(receiverId, sab);
    chrome.test.fail('SharedArrayBuffer should fail serialization');
  } catch (e) {
    chrome.test.assertTrue(e.message.includes('Could not serialize message'));
  }

  // Test sending a view backed by a `SharedArrayBuffer`. Because the buffer is
  // embedded inside a view, it bypasses the initial synchronous rejection in
  // the message sender layer. However, it will fail to deserialize on the
  // receiver side. The message listener will not be fired because of the
  // serialization error.
  const uint8ViewSab = new SharedArrayBuffer(16);
  const uint8View = new Uint8Array(uint8ViewSab);

  try {
    chrome.tabs.sendMessage(receiverId, uint8View);
  } catch (e) {
    chrome.test.fail('Uint8Array backed by SAB should not throw synchronously');
  }

  // The message sender will not be notified of the failure to deserialize in
  // the receiver so we can't directly verify that the failure to deserialize
  // happened. The next best thing is to verify that messages can continue to
  // be successfully sent to the tab to ensure the deserialization failure
  // isn't a fatal error for messaging.
  try {
    const response = await chrome.tabs.sendMessage(receiverId, 'ping');
    chrome.test.assertEq('pong', response);
  } catch (e) {
    chrome.test.fail(
        `Channel should be alive after invalid message: ${e.message}`);
  }

  chrome.runtime.sendMessage({testResult: 'success'});
})();
