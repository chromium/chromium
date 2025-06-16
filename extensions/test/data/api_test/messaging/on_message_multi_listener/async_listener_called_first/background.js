// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: the ordering is important here as first listener registered must be
// asynchronous to test the opposite scenario from the bug.

async function asyncCallSendResponse(sendResponse) {
  setTimeout(() => {
    sendResponse('Async response from background script');
  }, 1);
}

function onMessageAsyncListener(message, sender, sendResponse) {
  asyncCallSendResponse(sendResponse);
  return true;
}

chrome.runtime.onMessage.addListener(onMessageAsyncListener);

function onMessageSyncListener(message, sender, sendResponse) {
  return false;
}

chrome.runtime.onMessage.addListener(onMessageSyncListener);
