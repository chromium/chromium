// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class CustomError extends Error {
  constructor(...params) {
    super(...params);
  }
}

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  switch (message) {
    case 'Error':
      throw new Error('plain error message');
    case 'EvalError':
      throw new EvalError('eval error message');
    case 'ReferenceError':
      throw new ReferenceError('reference error message');
    case 'SyntaxError':
      throw new SyntaxError('syntax error message');
    case 'TypeError':
      throw new TypeError('type error message');
    case 'URIError':
      throw new URIError('uri error message');
    case 'AggregateError':
      throw new AggregateError(
          [new Error('an error')], 'aggregate error message');
    case 'CustomError':
      throw new CustomError('custom error message');
    default:
      chrome.test.fail('Unexpected test message: ' + message);
      break;
  }
});
