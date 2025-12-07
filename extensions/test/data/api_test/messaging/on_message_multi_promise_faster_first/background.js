// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onMessageListenerFasterPromise(message, unusedSender, sendResponse) {
  return new Promise(resolve => setTimeout(resolve, 1, 'faster promise'));
}
chrome.runtime.onMessage.addListener(onMessageListenerFasterPromise);

function onMessageListenerSlowerPromise(message, unusedSender, sendResponse) {
  return new Promise(resolve => setTimeout(resolve, 1000, 'slower promise'));
}
chrome.runtime.onMessage.addListener(onMessageListenerSlowerPromise);
