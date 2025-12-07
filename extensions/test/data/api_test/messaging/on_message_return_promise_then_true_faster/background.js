// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onMessageListenerFasterPromise(message, unusedSender, sendResponse) {
  return new Promise(resolve => setTimeout(resolve, 1000, 'slower promise'));
}
chrome.runtime.onMessage.addListener(onMessageListenerFasterPromise);

function onMessageListenerSlowerSendResponse(
    message, unusedSender, sendResponse) {
  setTimeout(sendResponse, 1, 'faster sendResponse');
  return true;
}
chrome.runtime.onMessage.addListener(onMessageListenerSlowerSendResponse);
