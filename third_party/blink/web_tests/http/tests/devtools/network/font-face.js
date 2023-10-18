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

  function onRequest(eventType, event) {
    var request = getRequestFromEvent(eventType, event);
    if (request.name() === 'done') {
      TestRunner.completeTest();
      return;
    }
    TestRunner.addResult(eventType + ': ' + request.name());
  }

  TestRunner.networkManager.addEventListener(
      SDK.NetworkManager.Events.RequestStarted, onRequest.bind(null, 'RequestStarted'));
  TestRunner.networkManager.addEventListener(
      SDK.NetworkManager.Events.RequestFinished,
      onRequest.bind(null, 'RequestFinished'));

  await TestRunner.addIframe('resources/font-face.html');
})();
