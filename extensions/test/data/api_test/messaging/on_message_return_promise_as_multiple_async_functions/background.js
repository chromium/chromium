// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function fasterAsyncFunctionResponse() {
  chrome.test.sendMessage('faster async function called');
  return 'faster async function (promise) response';
}
chrome.runtime.onMessage.addListener(fasterAsyncFunctionResponse);

async function slowerAsyncFunctionResponse() {
  chrome.test.sendMessage('slower async function called');
  // Return a never-resolving promise to ensure that the other async function's
  // promise always resolves first.
  return new Promise(() => {});
}
chrome.runtime.onMessage.addListener(slowerAsyncFunctionResponse);
