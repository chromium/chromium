// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener((message) => {
  chrome.test.assertTrue(self.crossOriginIsolated);
  chrome.test.assertFalse(typeof SharedArrayBuffer === 'undefined');

  chrome.test.assertTrue(message === null);
  chrome.runtime.sendMessage({testResult: 'success'});
});

// Notify background that we are ready to receive messages.
chrome.runtime.sendMessage({receiverReady: true});
