// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onMessageListener(message, sender) {
  if (message.receiverReady) {
    chrome.tabs.create({url: 'test_sender.html?receiverId=' + sender.tab.id});
  } else if (message.testResult) {
    chrome.runtime.onMessage.removeListener(onMessageListener);
    if (message.testResult === 'success') {
      chrome.test.succeed();
    } else {
      chrome.test.fail();
    }
  }
}

chrome.runtime.onMessage.addListener(onMessageListener);

chrome.test.runTests(
    [function testExtensionPageToExtensionPageSharedArrayBuffer() {
      chrome.tabs.create({url: 'test_receiver.html'});
    }]);
