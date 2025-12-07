// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onMessageListener(message) {
  switch (message) {
    case 'return promise reject with error object':
      return new Promise((unusedResolve, reject) => {
        reject(new Error('promise rejected error message'));
      });
    case 'return promise reject with custom object that has message key':
      return new Promise((unusedResolve, reject) => {
        reject({
          testKey: 'ignored value',
          message: 'promise rejected error message'
        });
      });
    case 'return promise reject with a non-error object':
      return new Promise((unusedResolve, reject) => {
        reject({
          testKey: 'ignored value',
        });
      });
    case 'return promise reject with a non-object':
      return new Promise((unusedResolve, reject) => {
        reject('non-object');
      });
    case 'return promise reject with no reject value':
      return new Promise((unusedResolve, reject) => {
        reject();
      });
    case 'return promise reject with undefined':
      return new Promise((unusedResolve, reject) => {
        reject(undefined);
      });
    default:
      chrome.test.fail('Unexpected test message: ' + message);
  }
}
chrome.runtime.onMessage.addListener(onMessageListener);
