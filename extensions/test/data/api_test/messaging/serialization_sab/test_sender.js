// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  chrome.test.assertTrue(self.crossOriginIsolated);
  chrome.test.assertFalse(typeof SharedArrayBuffer === 'undefined');

  const urlParams = new URLSearchParams(window.location.search);
  const receiverId = parseInt(urlParams.get('receiverId'));
  chrome.test.assertFalse(isNaN(receiverId));

  const sab = new SharedArrayBuffer(16);
  const view = new Int32Array(sab);
  view[0] = 1337;

  await chrome.tabs.sendMessage(receiverId, sab);
})();
