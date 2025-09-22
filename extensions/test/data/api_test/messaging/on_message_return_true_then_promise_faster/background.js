// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onMessageListenerFasterSendResponse(
    message, unusedSender, sendResponse) {
  setTimeout(sendResponse, 1000, 'slower sendResponse');
  return true;
}
chrome.runtime.onMessage.addListener(onMessageListenerFasterSendResponse);

function onMessageListenerSlowerPromise(message, unusedSender, sendResponse) {
  return new Promise(resolve => setTimeout(resolve, 1, 'faster promise'));
}
chrome.runtime.onMessage.addListener(onMessageListenerSlowerPromise);
