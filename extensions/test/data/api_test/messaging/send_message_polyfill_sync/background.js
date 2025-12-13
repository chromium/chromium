// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onMessageListener(unusedMessage, unusedSender, sendResponse) {
  sendResponse('synchronous response return');
  return;
}
chrome.runtime.onMessage.addListener(onMessageListener);
