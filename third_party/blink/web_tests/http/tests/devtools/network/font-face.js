// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that a used font-face is reported and an unused font-face is not reported.\n`);
  await TestRunner.showPanel('network');

  function getRequestFromEvent(eventType, event) {
    if (eventType === 'RequestStarted') {
      return event.data.request;
    } else {
      return event.data;
    }
  }

  const startedRequests = [];
  const finishedRequests = [];

  function onRequest(eventType, event) {
    var request = getRequestFromEvent(eventType, event);
    if (request.name() === 'done') {
      startedRequests.sort();
      finishedRequests.sort();
      for (const result of [...startedRequests, ...finishedRequests]) {
        TestRunner.addResult(result);
      }
      TestRunner.completeTest();
      return;
    }
    const result = eventType + ': ' + request.name();
    if (eventType === 'RequestStarted') {
      startedRequests.push(result);
    } else {
      finishedRequests.push(result);
    }
  }

  TestRunner.networkManager.addEventListener(
      SDK.NetworkManager.Events.RequestStarted, onRequest.bind(null, 'RequestStarted'));
  TestRunner.networkManager.addEventListener(
      SDK.NetworkManager.Events.RequestFinished,
      onRequest.bind(null, 'RequestFinished'));

  await TestRunner.addIframe('resources/font-face.html');
})();
