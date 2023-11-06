// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that XHR request type is detected on send.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function sendScriptRequest() {
          var script = document.createElement("script");
          script.src = "resources/empty-script.js";
          document.head.appendChild(script);
      }

      function sendXHRRequest() {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "resources/empty.html?xhr");
          xhr.send();
      }

      function createIFrame() {
          var iframe = document.createElement("iframe");
          iframe.src = "resources/empty.html?iframe";
          document.head.appendChild(iframe);
      }
  `);

  function step1() {
    requestName = 'empty.html?xhr';
    nextStep = step2;
    TestRunner.evaluateInPage('sendXHRRequest()');
  }

  function step2() {
    requestName = 'empty.html?iframe';
    nextStep = TestRunner.completeTest;
    TestRunner.evaluateInPage('createIFrame()');
  }

  function onRequest(event) {
    var request = event.data.request;
    if (request.name() !== requestName)
      return;
    requestName = undefined;
    TestRunner.addResult('');
    TestRunner.addResult('Request: ' + request.name());
    TestRunner.addResult('Type: ' + request.resourceType().name());
    nextStep();
  }

  TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestStarted, onRequest);

  var requestName = 'empty-script.js';
  var nextStep = step1;
  TestRunner.evaluateInPage('sendScriptRequest()');
})();
