// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  // Several tests that cover when there is a single registered listener for
  // runtime.onMessage and it synchronously throws several different types of
  // errors. The sender's promise should react similarly to
  // github.com/mozilla/webextension-polyfill.
  async function oneTimeMessageHandlerListenerThrowsErrors() {
    const test_cases = [
      {
        message: 'Error',
        error: 'Error: plain error message',
      },
      {
        message: 'EvalError',
        error: 'Error: eval error message',
      },
      {
        message: 'ReferenceError',
        error: 'Error: reference error message',
      },
      {
        message: 'SyntaxError',
        error: 'Error: syntax error message',
      },
      {
        message: 'TypeError',
        error: 'Error: type error message',
      },
      {
        message: 'URIError',
        error: 'Error: uri error message',
      },
      {
        message: 'AggregateError',
        error: 'Error: aggregate error message',
      },
      {
        message: 'CustomError',
        error: 'Error: custom error message',
      },
    ];

    for (const test_case of test_cases) {
      await chrome.test.assertPromiseRejects(
          chrome.runtime.sendMessage(test_case.message), test_case.error);
    }
    chrome.test.succeed();
  },

]);
