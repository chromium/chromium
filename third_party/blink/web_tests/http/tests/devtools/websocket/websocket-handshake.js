// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that WebSocket handshake information is passed to Web Inspector.\n`);
  await TestRunner.evaluateInPagePromise(`
      var ws;
      function sendMessages() {
          ws = new WebSocket('ws://localhost:8880/duplicated-headers');
      }
  `);

  function outputHeaders(name, headers) {
    var headersToOutput = [];
    for (var i = 0; i < headers.length; ++i) {
      if (headers[i].name === 'Sec-WebSocket-Key' || headers[i].name == 'Sec-WebSocket-Accept' ||
          headers[i].name == 'User-Agent' || headers[i].name == 'Accept-Encoding') {
        // We hide the header value of these headers because they can be flaky,
        // platform dependent, or irrelevant to this test.
        headersToOutput.push({name: headers[i].name, value: '***'});
      } else {
        headersToOutput.push(headers[i]);
      }
    }
    headersToOutput.sort(function(x, y) {
      function compare(x, y) {
        if (x < y) {
          return -1;
        } else if (x === y) {
          return 0;
        } else {
          return 1;
        }
      }
      return x.name === y.name ? compare(x.value, y.value) : compare(x.name, y.name);
    });
    console.log(name);
    for (var i = 0; i < headersToOutput.length; ++i) {
      console.log('    ' + headersToOutput[i].name + ': ' + headersToOutput[i].value);
    }
  }
  function onRequest(event) {
    if (event.data.statusCode === 101) {
      console.log('requestMethod: ' + event.data.requestMethod);
      outputHeaders('requestHeaders', event.data.requestHeaders());
      var headersText = event.data.requestHeadersText();
      var firstLine = headersText ? headersText.split('\r\n')[0] : headersText;
      console.log('requestHeadersText (first line): ' + firstLine);

      console.log('statusCode: ' + event.data.statusCode);
      console.log('statusText: ' + event.data.statusText);
      outputHeaders('responseHeaders', event.data.responseHeaders);
      headersText = event.data.responseHeadersText;
      firstLine = headersText ? headersText.split('\r\n')[0] : headersText;
      console.log('responseHeadersText (first line): ' + firstLine);
      TestRunner.completeTest();
    }
  }
  TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestUpdated, onRequest);
  TestRunner.evaluateInPage('sendMessages()');
})();
