// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function onMessageListenerReturnsPromiseAsAsyncFunction() {
  return 'async function promise return';
}

function doSomeAsyncThing() {
  return new Promise(
      resolve => setTimeout(resolve, 1, 'async function promise return'));
}

async function OnMessageListenerCallsSendResponseAsyncAfterPromise(
    sendResponse) {
  doSomeAsyncThing().then((result) => {
    sendResponse(result);
  });
  return 'outer promise return';
}

function onMessageListener(message, unusedSender, sendResponse) {
  switch (message) {
    case 'return promise resolve value string':
      return new Promise((resolve) => {
        resolve('promise resolved');
      });
    case 'return promise resolve value object':
      return new Promise((resolve) => {
        resolve({testKey: 'promise resolved'});
      });
    case 'return promise resolve an error':
      return new Promise((resolve) => {
        resolve(new Error('promise resolve error object message'));
      });
    case 'return promise as an async function':
      return onMessageListenerReturnsPromiseAsAsyncFunction();
    case 'call sendResponse asynchronously after returned promise resolves':
      return OnMessageListenerCallsSendResponseAsyncAfterPromise(sendResponse);
    case 'return promise after synchronous sendResponse() is called':
      sendResponse('synchronous response return');
      return new Promise(resolve => resolve('promise resolved'));
    default:
      chrome.test.fail('Unexpected test message: ' + message);
  }
}
chrome.runtime.onMessage.addListener(onMessageListener);
